#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_main.h>

#include "bios.h"
#include "config.h"
#include "intelliemu.h"
#include "menu.h"
#include "recent.h"
#include "remap.h"
#include "xbrz.h"

#ifdef _WIN32
#define CORE_FILE_NAME "freeintv_libretro.dll"
#else
#define CORE_FILE_NAME "freeintv_libretro.so"
#endif

#ifndef DEFAULT_CORE
#define DEFAULT_CORE "./" CORE_FILE_NAME
#endif

#define DEFAULT_WIDTH 352
#define DEFAULT_HEIGHT 224
#define DEBUG_CHAR_W 8
#define DEBUG_CHAR_H 8
#define ABOUT_PAD 8
#define ABOUT_LINE_H 12
#define ABOUT_COLS 96
#define STAGE_FRAMES 4096
#define AUDIO_LEAD_FRAMES 3
#define DIALOG_RESULT 0
#define DIALOG_ERROR 1
#define OPEN_ROM_ERROR "Could not load ROM"
#define MAX_SCREENSHOT 999

struct options
{
   const char *core_path;
   const char *system_dir;
   const char *rom;
   long scale;
   long max_frames;
   bool fullscreen;
   bool bankswitch;
   int xbrz;
};

static core_t core;
static const char *core_path;
static const char *system_dir;
static struct bios_files bios;
static char *rom_file;
static long window_scale;
static bool run_forced_fullscreen;
static int xbrz_override = -1;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static unsigned texture_w, texture_h;
static unsigned pic_w = DEFAULT_WIDTH, pic_h = DEFAULT_HEIGHT;
static bool audio_subsystem;
static SDL_AudioStream *audio;
static SDL_AudioDeviceID audio_device;
static int audio_core_rate;
static int audio_lead_bytes;
static uint64_t frame_ns;
static uint64_t next_frame_ns;
static bool quit_requested;
static bool paused;
static struct settings settings;

/* The --xbrz/--no-xbrz flag overrides the saved setting for this run only. */
static bool xbrz_on(void)
{
   return xbrz_override >= 0 ? xbrz_override : settings.xbrz;
}
static bool about_open;
static bool dialog_pending;
static Uint32 open_rom_event;
static char status[64];
#define NOTICE_MS 4000
static char notice[64];
static Uint64 notice_until_ms;
static struct recent_list recent;
static int16_t stage[STAGE_FRAMES * 2];
static size_t stage_frames;
static bool audio_error_reported;
/* Packed copy of the last core frame (4 bytes per pixel), for screenshots and xBRZ changes. */
static void *last_frame;
static size_t last_frame_size;
static unsigned last_w, last_h;
static void *xbrz_frame;
static size_t xbrz_frame_size;

static bool game_running(void)
{
   return core.game_loaded;
}

static const char *port_name(int port)
{
   return port == 0 ? "Right" : "Left";
}

/* 4:3 mode stretches the picture vertically to pic_w * 3 / 4 so its shape is 4:3. */
static unsigned view_height(void)
{
   return settings.aspect_4_3 ? pic_w * 3 / 4 : pic_h;
}

static bool fullscreen_on(void)
{
   return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

/* The menu bar is hidden in fullscreen, so the picture starts at the top of the canvas. */
static unsigned bar_height(void)
{
   return fullscreen_on() ? 0 : MENU_BAR_HEIGHT;
}

/* The menu bar is drawn above the picture, so the logical canvas is one bar taller. */
static void update_presentation(void)
{
   if (renderer)
      SDL_SetRenderLogicalPresentation(renderer, (int)pic_w, (int)(view_height() + bar_height()),
            settings.aspect_4_3 ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_STRETCH);
}

static bool reserve(void **buf, size_t *size, size_t need)
{
   void *grown;

   if (*size >= need)
      return true;

   grown = SDL_realloc(*buf, need);
   if (!grown)
      return false;

   *buf = grown;
   *size = need;
   return true;
}

/* Uploads a packed XRGB frame. With xBRZ on, the texture is twice as large in each direction. */
static void present_frame(const void *frame, unsigned width, unsigned height)
{
   unsigned scale = xbrz_on() ? XBRZ_SCALE : 1;
   unsigned tex_w = width * scale;
   unsigned tex_h = height * scale;

   if (!texture || texture_w != tex_w || texture_h != tex_h)
   {
      if (texture)
         SDL_DestroyTexture(texture);

      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, (int)tex_w, (int)tex_h);
      if (!texture)
      {
         fprintf(stderr, "cannot create video texture: %s\n", SDL_GetError());
         texture_w = 0;
         texture_h = 0;
         return;
      }

      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
      texture_w = tex_w;
      texture_h = tex_h;
   }

   if (xbrz_on())
   {
      if (!reserve(&xbrz_frame, &xbrz_frame_size, (size_t)tex_w * tex_h * 4))
         return;

      xbrz_scale(frame, (size_t)width * 4, xbrz_frame, (int)width, (int)height);
      SDL_UpdateTexture(texture, NULL, xbrz_frame, (int)(tex_w * 4));
   }
   else
      SDL_UpdateTexture(texture, NULL, frame, (int)(width * 4));
}

static bool remember_frame(const void *data, unsigned width, unsigned height, size_t pitch)
{
   size_t row = (size_t)width * 4;
   unsigned y;

   if (!reserve(&last_frame, &last_frame_size, row * height))
      return false;

   for (y = 0; y < height; y++)
      SDL_memcpy((unsigned char *)last_frame + y * row, (const unsigned char *)data + y * pitch, row);

   last_w = width;
   last_h = height;
   return true;
}

void frontend_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
   /* A NULL frame means the core wants the previous frame shown again. */
   if (!data)
      return;

   if (pic_w != width || pic_h != height)
   {
      pic_w = width;
      pic_h = height;
      update_presentation();
   }

   if (remember_frame(data, width, height, pitch))
      present_frame(last_frame, width, height);
}

static void audio_flush(void)
{
   size_t bytes = stage_frames * 2 * sizeof(stage[0]);

   if (audio && bytes > 0 && !SDL_PutAudioStreamData(audio, stage, (int)bytes) && !audio_error_reported)
   {
      audio_error_reported = true;
      fprintf(stderr, "audio: %s\n", SDL_GetError());
   }

   stage_frames = 0;
}

static void audio_push(int16_t left, int16_t right)
{
   stage[stage_frames * 2] = left;
   stage[stage_frames * 2 + 1] = right;

   if (++stage_frames == STAGE_FRAMES)
      audio_flush();
}

void frontend_audio_sample(int16_t left, int16_t right)
{
   audio_push(left, right);
}

size_t frontend_audio_sample_batch(const int16_t *data, size_t frames)
{
   size_t i;

   for (i = 0; i < frames; i++)
      audio_push(data[i * 2], data[i * 2 + 1]);

   return frames;
}

static float audio_gain(void)
{
   return settings.muted ? 0.0f : settings.volume / 100.0f;
}

static void audio_close(void);

/* The core always produces audio_core_rate; SDL resamples it to the device rate. */
static void audio_open(int core_rate)
{
   SDL_AudioSpec src = { .format = SDL_AUDIO_S16, .channels = 2, .freq = core_rate };
   SDL_AudioSpec dst = { .format = SDL_AUDIO_S16, .channels = 2, .freq = settings.sample_rate };

   audio_core_rate = core_rate;
   if (!audio_subsystem)
      return;

   audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &dst);
   if (audio_device)
      audio = SDL_CreateAudioStream(&src, NULL);
   if (!audio || !SDL_BindAudioStream(audio_device, audio))
   {
      fprintf(stderr, "audio disabled, pacing by timer: %s\n", SDL_GetError());
      audio_close();
      return;
   }

   SDL_SetAudioStreamGain(audio, audio_gain());
   SDL_ResumeAudioDevice(audio_device);
}

static void audio_close(void)
{
   if (audio)
      SDL_DestroyAudioStream(audio);
   if (audio_device)
      SDL_CloseAudioDevice(audio_device);
   audio = NULL;
   audio_device = 0;
}

static void set_title(void)
{
   char *title = NULL;
   const char *name = rom_file ? path_basename(rom_file) : NULL;

   if (!window)
      return;

   if (!name || SDL_asprintf(&title, "Intelliemu - %s", name) < 0)
      SDL_SetWindowTitle(window, "Intelliemu");
   else
      SDL_SetWindowTitle(window, title);

   SDL_free(title);
}

static void resize_window(void)
{
   if (!window || (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN))
      return;

   SDL_SetWindowSize(window, (int)(pic_w * (unsigned)window_scale),
         (int)((view_height() + MENU_BAR_HEIGHT) * (unsigned)window_scale));
}

static void close_game(void)
{
   input_release_all();
   core_close(&core);

   audio_close();
   stage_frames = 0;

   if (texture)
   {
      SDL_DestroyTexture(texture);
      texture = NULL;
   }
   texture_w = 0;
   texture_h = 0;

   SDL_free(rom_file);
   rom_file = NULL;
   paused = false;
   pic_w = DEFAULT_WIDTH;
   pic_h = DEFAULT_HEIGHT;

   update_presentation();
   set_title();
}

static bool open_rom(const char *path, bool bankswitch)
{
   /* The copy comes first: path may point into the recent list, which recent_add rewrites. */
   char *copy = SDL_strdup(path);
   struct retro_system_av_info av;

   if (!copy)
      return false;

   close_game();

   if (!core_open(&core, core_path, system_dir, &bios))
      goto fail;

   if (!core_load_game(&core, copy, bankswitch))
   {
      core_close(&core);
      goto fail;
   }

   core.get_system_av_info(&av);
   frame_ns = (uint64_t)(1000000000.0 / av.timing.fps);
   next_frame_ns = SDL_GetTicksNS();
   audio_lead_bytes = (int)(av.timing.sample_rate / av.timing.fps * AUDIO_LEAD_FRAMES) * 2 * (int)sizeof(int16_t);
   pic_w = av.geometry.base_width;
   pic_h = av.geometry.base_height;

   audio_open((int)av.timing.sample_rate);

   rom_file = copy;
   status[0] = '\0';
   /* Bank-switched carts are not recorded: the recent list reopens plain ROMs only. */
   if (!bankswitch)
      recent_add(&recent, rom_file);

   update_presentation();
   resize_window();
   set_title();
   return true;

fail:
   SDL_strlcpy(status, OPEN_ROM_ERROR, sizeof(status));
   SDL_free(copy);
   return false;
}

static void SDLCALL open_dialog_done(void *userdata, const char *const *filelist, int filter)
{
   SDL_Event event;
   char *path = NULL;

   (void)filter;

   if (!filelist)
   {
      fprintf(stderr, "file dialog failed: %s\n", SDL_GetError());
   }
   else if (filelist[0])
   {
      path = SDL_strdup(filelist[0]);
   }

   SDL_zero(event);
   event.type = open_rom_event;
   event.user.code = filelist ? DIALOG_RESULT : DIALOG_ERROR;
   event.user.data1 = path;
   event.user.data2 = userdata;
   if (!SDL_PushEvent(&event))
      SDL_free(path);
}

static void show_open_dialog(bool bankswitch)
{
   static const SDL_DialogFileFilter rom_filters[] = { { "Intellivision ROMs", "int;bin;rom" } };
   static const SDL_DialogFileFilter bank_filters[] = { { "Bank-switched ROMs", "cfg;bin;int" } };
   const SDL_DialogFileFilter *filters = bankswitch ? bank_filters : rom_filters;

   if (dialog_pending)
      return;

   dialog_pending = true;
   SDL_ShowOpenFileDialog(open_dialog_done, (void *)(uintptr_t)bankswitch, window, filters, 1, settings.open_dir, false);
}

static bool frame_due(void)
{
   if (audio)
      return SDL_GetAudioStreamQueued(audio) < audio_lead_bytes;

   return SDL_GetTicksNS() >= next_frame_ns;
}

static void run_frame(void)
{
   uint64_t now;

   core.run();
   input_after_run();
   audio_flush();

   if (!audio)
   {
      now = SDL_GetTicksNS();
      next_frame_ns += frame_ns;
      if (next_frame_ns < now)
         next_frame_ns = now;
   }
}

static void fill_menu_state(struct menu_state *state)
{
   state->game_running = game_running();
   state->paused = paused;
   state->fullscreen = window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
   state->muted = settings.muted;
   state->sample_rate = settings.sample_rate;
   state->volume = settings.volume;
   state->aspect_4_3 = settings.aspect_4_3;
   state->xbrz = xbrz_on();
   state->ecs_keyboard = settings.ecs_keyboard;
   state->state_slot = settings.state_slot;
   state->canvas_width = (int)pic_w;
   state->recent = &recent;
}

static void draw_centered(float y, const char *text)
{
   float x = ((float)pic_w - (float)(strlen(text) * DEBUG_CHAR_W)) / 2;

   SDL_RenderDebugText(renderer, x < 0 ? 0 : x, y, text);
}

static const char about_text[] =
   "IntelliEmu Version 1.0\n"
   "\n"
   "Designed by R.Shaklfrd and Claude Haiku\n"
   "\n"
   "Originally based on David Richardson's FreeIntv\n"
   "Uses IntelliVoice code designed by Joe Zbiciak and Oscar Toledo G\n"
   "\n"
   "Released under GPL2 Licensing";

/* Word-wraps about_text to cols characters per line. Blank lines in the text are kept. */
static void draw_about_text(float x, float y, int cols)
{
   char line[ABOUT_COLS + 1];
   const char *para = about_text;

   for (;;)
   {
      const char *end = strchr(para, '\n');
      const char *w = para;
      int len = 0;

      if (!end)
         end = para + strlen(para);

      while (w < end)
      {
         const char *word;
         size_t wl;

         while (w < end && *w == ' ')
            w++;
         word = w;
         while (w < end && *w != ' ')
            w++;
         wl = (size_t)(w - word);
         if (wl == 0)
            break;
         if (wl > (size_t)cols)
            wl = (size_t)cols;

         if (len > 0 && len + 1 + (int)wl > cols)
         {
            line[len] = '\0';
            SDL_RenderDebugText(renderer, x, y, line);
            y += ABOUT_LINE_H;
            len = 0;
         }
         if (len > 0)
            line[len++] = ' ';
         memcpy(line + len, word, wl);
         len += (int)wl;
      }

      line[len] = '\0';
      SDL_RenderDebugText(renderer, x, y, line);
      y += ABOUT_LINE_H;

      if (!*end)
         break;
      para = end + 1;
   }
}

static void draw_about(unsigned bar)
{
   const float top = (float)bar;
   const float height = (float)view_height();
   SDL_FRect panel = { 0, top, (float)pic_w, height };
   int cols = ((int)pic_w - 2 * ABOUT_PAD) / DEBUG_CHAR_W;

   if (cols > ABOUT_COLS)
      cols = ABOUT_COLS;

   SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
   SDL_RenderFillRect(renderer, &panel);

   SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
   draw_about_text(ABOUT_PAD, top + ABOUT_PAD, cols);
   SDL_RenderDebugText(renderer, ABOUT_PAD, top + height - ABOUT_PAD - DEBUG_CHAR_H,
         "Press any key or click to close");
}

static void render(void)
{
   struct menu_state state;
   unsigned bar = bar_height();
   SDL_FRect dst = { 0, (float)bar, (float)pic_w, (float)view_height() };
   float mid = (float)bar + (float)view_height() / 2;

   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderClear(renderer);

   SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
   if (texture)
   {
      SDL_RenderTexture(renderer, texture, NULL, &dst);
      if (paused)
         SDL_RenderDebugText(renderer, 4, (float)bar + 4, "PAUSED");
   }
   else
   {
      draw_centered(mid - 12, status[0] ? status : "No game loaded");
      draw_centered(mid + 4, "File > Open ROM... (Ctrl+O)");
   }

   if (notice[0] && SDL_GetTicks() < notice_until_ms)
      draw_centered((float)bar + (float)view_height() - 12, notice);

   if (remap_is_open())
   {
      const char *pad_names[PLAYER_COUNT];
      int port;

      for (port = 0; port < PLAYER_COUNT; port++)
         pad_names[port] = input_pad_name(port);

      remap_render(renderer, (int)pic_w, (int)(view_height() + bar), (int)bar, pad_names);
   }

   if (about_open)
      draw_about(bar);

   if (!bar)
   {
      SDL_RenderPresent(renderer);
      return;
   }

   fill_menu_state(&state);
   menu_render(renderer, &state);
   SDL_RenderPresent(renderer);
}


static int stem_length(const char *path)
{
   const char *sep = strrchr(path, '/');
   const char *bsep = strrchr(path, '\\');
   const char *dot = strrchr(path, '.');

   if (bsep && (!sep || bsep > sep))
      sep = bsep;

   if (!dot || (sep && dot < sep))
      return (int)strlen(path);

   return (int)(dot - path);
}

static char *state_path(int slot)
{
   char *path = NULL;

   if (SDL_asprintf(&path, "%.*s.state%d", stem_length(rom_file), rom_file, slot) < 0)
      return NULL;

   return path;
}

static bool write_file(const char *path, const void *data, size_t size)
{
   SDL_IOStream *io = SDL_IOFromFile(path, "wb");
   bool ok;

   if (!io)
   {
      fprintf(stderr, "cannot open '%s': %s\n", path, SDL_GetError());
      return false;
   }

   ok = SDL_WriteIO(io, data, size) == size;
   if (!SDL_CloseIO(io))
      ok = false;

   return ok;
}

static bool read_file(const char *path, void *data, size_t size)
{
   SDL_IOStream *io = SDL_IOFromFile(path, "rb");
   bool ok = false;

   if (!io)
   {
      fprintf(stderr, "no saved state at '%s'\n", path);
      return false;
   }

   if (SDL_GetIOSize(io) != (Sint64)size)
      fprintf(stderr, "'%s' is not a state for this core\n", path);
   else
      ok = SDL_ReadIO(io, data, size) == size;

   SDL_CloseIO(io);
   return ok;
}

static void save_state(void)
{
   size_t size;
   void *buf;
   char *path;

   if (!game_running())
      return;

   size = core.serialize_size();
   buf = SDL_malloc(size);
   path = state_path(settings.state_slot);

   if (buf && path && core.serialize(buf, size) && write_file(path, buf, size))
      fprintf(stderr, "saved state slot %d to %s\n", settings.state_slot, path);
   else
      fprintf(stderr, "failed to save state slot %d\n", settings.state_slot);

   SDL_free(buf);
   SDL_free(path);
}

static void load_state(void)
{
   size_t size;
   void *buf;
   char *path;

   if (!game_running())
      return;

   size = core.serialize_size();
   buf = SDL_malloc(size);
   path = state_path(settings.state_slot);

   if (buf && path && read_file(path, buf, size) && core.unserialize(buf, size))
   {
      if (audio)
         SDL_ClearAudioStream(audio);
      fprintf(stderr, "loaded state slot %d from %s\n", settings.state_slot, path);
   }
   else
   {
      fprintf(stderr, "failed to load state slot %d\n", settings.state_slot);
   }

   SDL_free(buf);
   SDL_free(path);
}

static void toggle_fullscreen(void)
{
   SDL_WindowFlags flags = SDL_GetWindowFlags(window);

   settings.fullscreen = !(flags & SDL_WINDOW_FULLSCREEN);
   SDL_SetWindowFullscreen(window, settings.fullscreen);
   settings_save(&settings);
}

/* Runs each frame so fullscreen changes from any source (hotkey, menu, window manager) update the layout.
 * Window-manager changes are saved as the new default. A forced -f start is not saved. */
static void sync_fullscreen(void)
{
   static bool shown_fullscreen;
   bool now = fullscreen_on();

   if (now == shown_fullscreen)
      return;

   shown_fullscreen = now;
   if (now)
      menu_close();
   else
      run_forced_fullscreen = false;
   if (now != settings.fullscreen && !(run_forced_fullscreen && now))
   {
      settings.fullscreen = now;
      settings_save(&settings);
   }
   update_presentation();
}

static void toggle_mute(void)
{
   settings.muted = !settings.muted;
   if (audio)
      SDL_SetAudioStreamGain(audio, audio_gain());
   settings_save(&settings);
}

static void set_volume(int volume)
{
   settings.volume = volume;
   if (audio)
      SDL_SetAudioStreamGain(audio, audio_gain());
   settings_save(&settings);
}

static void set_sample_rate(int rate)
{
   settings.sample_rate = rate;
   if (audio)
   {
      audio_close();
      audio_open(audio_core_rate);
   }
   settings_save(&settings);
}

static void toggle_aspect(void)
{
   settings.aspect_4_3 = !settings.aspect_4_3;
   update_presentation();
   settings_save(&settings);
}

static void toggle_xbrz(void)
{
   settings.xbrz = !xbrz_on();
   xbrz_override = -1;
   if (texture && last_frame && game_running())
      present_frame(last_frame, last_w, last_h);
   settings_save(&settings);
}

static void toggle_ecs_keyboard(void)
{
   settings.ecs_keyboard = !settings.ecs_keyboard;
   input_set_ecs_keyboard(settings.ecs_keyboard);
   settings_save(&settings);
}

static void notify(const char *text)
{
   SDL_strlcpy(notice, text, sizeof(notice));
   notice_until_ms = SDL_GetTicks() + NOTICE_MS;
}

/* Saves the native core frame (without the menu or filter) as the first free screenshot_NNN.png. */
static void take_screenshot(void)
{
   SDL_PathInfo info;
   SDL_Surface *surface = NULL;
   char name[32] = "";
   int n;

   if (!game_running() || !last_frame)
   {
      notify("Start a game to take a screenshot");
      return;
   }

   for (n = 1; n <= MAX_SCREENSHOT; n++)
   {
      SDL_snprintf(name, sizeof(name), "screenshot_%03d.png", n);
      if (!SDL_GetPathInfo(name, &info))
         break;
   }

   if (n <= MAX_SCREENSHOT)
      surface = SDL_CreateSurfaceFrom((int)last_w, (int)last_h, SDL_PIXELFORMAT_XRGB8888, last_frame,
            (int)(last_w * 4));

   if (surface && SDL_SavePNG(surface, name))
   {
      SDL_snprintf(notice, sizeof(notice), "Saved %s", name);
      fprintf(stderr, "saved screenshot to %s\n", name);
   }
   else
   {
      notify("Screenshot failed");
      fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
   }

   if (surface)
      SDL_DestroySurface(surface);
   notice_until_ms = SDL_GetTicks() + NOTICE_MS;
}

static void swap_gamepads(void)
{
   const char *right;
   const char *left;

   if (!input_swap_gamepads())
   {
      SDL_strlcpy(notice, "No gamepad connected to swap", sizeof(notice));
      notice_until_ms = SDL_GetTicks() + NOTICE_MS;
      return;
   }

   /* Gamepad bindings travel with their pad. Keyboard bindings stay with the port. */
   for (int i = 0; i < CONTROL_COUNT; i++)
   {
      int pad = settings.bindings[0].pads[i];

      settings.bindings[0].pads[i] = settings.bindings[1].pads[i];
      settings.bindings[1].pads[i] = pad;
   }
   input_set_bindings(settings.bindings);
   input_get_gamepad_ids(settings.gamepad_id);
   settings_save(&settings);

   right = input_pad_name(0);
   left = input_pad_name(1);
   SDL_snprintf(notice, sizeof(notice), "Right: %s   Left: %s",
         right ? (right[0] ? right : "unnamed") : "none",
         left ? (left[0] ? left : "unnamed") : "none");
   notice_until_ms = SDL_GetTicks() + NOTICE_MS;
}

static void apply_bindings(void)
{
   input_set_bindings(settings.bindings);
   settings_save(&settings);
}

static void cycle_state_slot(void)
{
   settings.state_slot = settings.state_slot % MAX_STATE_SLOT + 1;
   fprintf(stderr, "state slot %d\n", settings.state_slot);
   settings_save(&settings);
}

static void show_about(void)
{
   about_open = true;
}

static void handle_menu_action(const struct menu_action *action)
{
   switch (action->command)
   {
      case MENU_OPEN_ROM:
         show_open_dialog(false);
         break;
      case MENU_OPEN_BANK_ROM:
         show_open_dialog(true);
         break;
      case MENU_CLOSE_ROM:
         close_game();
         status[0] = '\0';
         break;
      case MENU_RECENT:
         {
            char *path = SDL_strdup(recent.paths[action->arg]);

            if (path)
               open_rom(path, false);
            SDL_free(path);
         }
         break;
      case MENU_ABOUT:
         menu_close();
         show_about();
         break;
      case MENU_EXIT:
         quit_requested = true;
         break;
      case MENU_RESET:
         if (game_running())
            core.reset();
         break;
      case MENU_TOGGLE_PAUSE:
         if (game_running())
            paused = !paused;
         break;
      case MENU_SAVE_STATE:
         save_state();
         break;
      case MENU_LOAD_STATE:
         load_state();
         break;
      case MENU_NEXT_SLOT:
         cycle_state_slot();
         break;
      case MENU_TOGGLE_FULLSCREEN:
         toggle_fullscreen();
         break;
      case MENU_TOGGLE_MUTE:
         toggle_mute();
         break;
      case MENU_SET_SAMPLE_RATE:
         set_sample_rate(action->arg);
         break;
      case MENU_SET_VOLUME:
         set_volume(action->arg);
         break;
      case MENU_TOGGLE_ASPECT:
         toggle_aspect();
         break;
      case MENU_TOGGLE_XBRZ:
         toggle_xbrz();
         break;
      case MENU_TOGGLE_ECS_KEYBOARD:
         toggle_ecs_keyboard();
         break;
      case MENU_SCREENSHOT:
         take_screenshot();
         break;
      case MENU_REMAP_RIGHT_KEYS:
         menu_close();
         remap_open(settings.bindings, 0, REMAP_KEYBOARD);
         break;
      case MENU_REMAP_LEFT_KEYS:
         menu_close();
         remap_open(settings.bindings, 1, REMAP_KEYBOARD);
         break;
      case MENU_REMAP_RIGHT_PAD:
         menu_close();
         remap_open(settings.bindings, 0, REMAP_GAMEPAD);
         break;
      case MENU_REMAP_LEFT_PAD:
         menu_close();
         remap_open(settings.bindings, 1, REMAP_GAMEPAD);
         break;
      case MENU_SWAP_GAMEPADS:
         swap_gamepads();
         break;
      case MENU_RESET_KEYS:
         controls_reset(settings.bindings);
         apply_bindings();
         break;
      default:
         break;
   }
}

static enum menu_command hotkey_command(const SDL_KeyboardEvent *key)
{
   if (key->mod & SDL_KMOD_CTRL)
   {
      switch (key->scancode)
      {
         case SDL_SCANCODE_O:
            return MENU_OPEN_ROM;
         case SDL_SCANCODE_W:
            return MENU_CLOSE_ROM;
         case SDL_SCANCODE_Q:
            return MENU_EXIT;
         default:
            break;
      }
   }

   switch (key->scancode)
   {
      case SDL_SCANCODE_F1:
         return MENU_SAVE_STATE;
      case SDL_SCANCODE_F5:
         return MENU_LOAD_STATE;
      case SDL_SCANCODE_F9:
         return MENU_NEXT_SLOT;
      case SDL_SCANCODE_F10:
         return MENU_RESET;
      case SDL_SCANCODE_F11:
         return MENU_TOGGLE_FULLSCREEN;
      case SDL_SCANCODE_F12:
         return MENU_SCREENSHOT;
      default:
         return MENU_NONE;
   }
}

static void handle_key(const SDL_KeyboardEvent *key)
{
   struct menu_action action = { MENU_NONE, 0 };

   if (key->scancode == SDL_SCANCODE_ESCAPE)
   {
      if (menu_is_open())
         menu_close();
      else
         quit_requested = true;
      return;
   }

   action.command = hotkey_command(key);
   handle_menu_action(&action);
}

static void handle_pointer(float window_x, float window_y, bool click)
{
   struct menu_state state;
   struct menu_action action = { MENU_NONE, 0 };
   float x, y;

   if (remap_is_open() || fullscreen_on())
      return;

   if (!SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &x, &y))
      return;

   if (!click)
   {
      menu_hover(x, y);
      return;
   }

   fill_menu_state(&state);
   if (menu_click(&state, x, y, &action))
      handle_menu_action(&action);
}

/* Keeps the folder of path (with its trailing separator) as the dialog's next start folder. */
static void remember_open_dir(const char *path)
{
   const char *slash = strrchr(path, '/');
   const char *bslash = strrchr(path, '\\');
   char *dir;

   if (bslash && (!slash || bslash > slash))
      slash = bslash;
   if (!slash)
      return;

   dir = SDL_strndup(path, (size_t)(slash - path) + 1);
   if (!dir)
      return;

   SDL_free(settings.open_dir);
   settings.open_dir = dir;
   settings_save(&settings);
}

static void handle_dialog_result(const SDL_Event *event)
{
   char *path = event->user.data1;
   bool bankswitch = event->user.data2 != NULL;

   dialog_pending = false;

   if (event->user.code == DIALOG_ERROR)
      SDL_strlcpy(status, "File dialog unavailable", sizeof(status));
   else if (path)
   {
      remember_open_dir(path);
      open_rom(path, bankswitch);
   }

   SDL_free(path);
}

static void announce_gamepad(SDL_JoystickID id)
{
   int port = input_pad_port(id);
   const char *name = port >= 0 ? input_pad_name(port) : NULL;

   if (port < 0)
      SDL_strlcpy(notice, "Gamepad not assigned: both ports already have one", sizeof(notice));
   else
      SDL_snprintf(notice, sizeof(notice), "%s gamepad: %s", port_name(port), name && name[0] ? name : "unnamed");

   notice_until_ms = SDL_GetTicks() + NOTICE_MS;
}

static void handle_event(const SDL_Event *event)
{
   if (event->type == SDL_EVENT_QUIT)
      quit_requested = true;
   else if (about_open && (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
            event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN))
   {
      about_open = false;
      return;
   }
   else if (event->type == SDL_EVENT_KEY_DOWN && remap_is_open())
   {
      if (remap_handle_key(&event->key))
         apply_bindings();
   }
   else if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
      handle_key(&event->key);
   else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
      handle_pointer(event->button.x, event->button.y, true);
   else if (event->type == SDL_EVENT_MOUSE_MOTION)
      handle_pointer(event->motion.x, event->motion.y, false);
   else if (event->type == open_rom_event)
      handle_dialog_result(event);

   if (remap_wants_pad() && remap_handle_pad(input_pad_press_from_event(event)))
      apply_bindings();

   input_handle_event(event);

   if (event->type == SDL_EVENT_GAMEPAD_ADDED)
      announce_gamepad(event->gdevice.which);
}

static void update_input_block(void)
{
   input_set_blocked(menu_is_open() || about_open || remap_is_open() || paused || !game_running());
}

static void main_loop(long max_frames)
{
   long frames_run = 0;

   next_frame_ns = SDL_GetTicksNS();

   while (!quit_requested)
   {
      SDL_Event event;

      while (SDL_PollEvent(&event))
         handle_event(&event);

      sync_fullscreen();
      update_input_block();

      if (game_running() && !paused && frame_due())
      {
         run_frame();
         frames_run++;
      }
      else
      {
         SDL_Delay(1);
      }

      render();

      if (max_frames >= 0 && frames_run >= max_frames)
         quit_requested = true;
   }
}


static int run(const struct options *opt)
{
   int status_code = 1;

   core_path = opt->core_path;
   system_dir = opt->system_dir;
   window_scale = opt->scale;
   run_forced_fullscreen = opt->fullscreen;
   xbrz_override = opt->xbrz;

   if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
   {
      fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return 1;
   }

   input_set_gamepad_ids(settings.gamepad_id);
   input_init();
   input_set_bindings(settings.bindings);
   input_set_ecs_keyboard(settings.ecs_keyboard);
   open_rom_event = SDL_RegisterEvents(1);
   if (!open_rom_event)
   {
      fprintf(stderr, "cannot register events: %s\n", SDL_GetError());
      goto cleanup;
   }

   audio_subsystem = SDL_InitSubSystem(SDL_INIT_AUDIO);
   if (!audio_subsystem)
      fprintf(stderr, "audio disabled, pacing by timer: %s\n", SDL_GetError());

   recent_load(&recent);

   window = SDL_CreateWindow("Intelliemu",
         (int)(DEFAULT_WIDTH * (unsigned)opt->scale),
         (int)((view_height() + MENU_BAR_HEIGHT) * (unsigned)opt->scale),
         SDL_WINDOW_RESIZABLE);
   if (!window)
   {
      fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
      goto cleanup;
   }

   renderer = SDL_CreateRenderer(window, NULL);
   if (!renderer)
   {
      fprintf(stderr, "cannot create renderer: %s\n", SDL_GetError());
      goto cleanup;
   }

   if (settings.fullscreen || opt->fullscreen)
      SDL_SetWindowFullscreen(window, true);

   SDL_SetRenderVSync(renderer, 1);
   update_presentation();
   set_title();

   if (opt->rom && !open_rom(opt->rom, opt->bankswitch))
      goto cleanup;

   main_loop(opt->max_frames);
   status_code = 0;

cleanup:
   input_release_all();
   input_shutdown();
   close_game();
   if (texture)
      SDL_DestroyTexture(texture);
   if (renderer)
      SDL_DestroyRenderer(renderer);
   if (window)
      SDL_DestroyWindow(window);
   recent_free(&recent);
   SDL_free(last_frame);
   SDL_free(xbrz_frame);
   SDL_Quit();
   return status_code;
}

static void usage(const char *prog)
{
   fprintf(stderr,
         "usage: %s [options] [GAME]\n"
         "  -c, --core PATH    libretro core library (default: %s)\n"
         "  -s, --system DIR   extra folder searched for BIOS files (default: from config.ini)\n"
         "  -w, --scale N      initial window scale, 1-%d (default: from config.ini)\n"
         "  -f, --fullscreen   start in fullscreen\n"
         "  -x, --xbrz         start with the xBRZ filter on\n"
         "      --no-xbrz      start with the xBRZ filter off\n"
         "  -b, --bankswitch   load GAME as a bank-switched cart (.cfg/.bin/.int)\n"
         "  -d, --frames N     exit after N frames (requires GAME; for testing)\n"
         "  -h, --help         show this help\n"
         "\n"
         "settings: $XDG_CONFIG_HOME/Intelliemu/config.ini (default ~/.config/Intelliemu/config.ini)\n"
         "          -s, -w, -f, -x, and --no-xbrz apply to this run only\n"
         "menu: File (open, close, recent ROMs, about, exit), Emulation (reset, pause, remap, swap gamepads, restore bindings),\n"
         "      State (save, load, slot), Video (fullscreen, 4:3 aspect, xBRZ filter, screenshot), Audio (mute)\n"
         "hotkeys: Ctrl+O open ROM, Ctrl+W close ROM, Ctrl+Q quit,\n"
         "         F1 save state, F5 load state, F9 next slot, F10 reset,\n"
         "         F11 fullscreen, F12 screenshot, Esc close menu or quit\n"
         "keyboard defaults: Right = WASD disc and 0-9 keypad; Left = arrows disc and numpad keypad\n"
         "bindings: Emulation > Remap Right/Left keyboard or gamepad\n",
         prog, DEFAULT_CORE, MAX_SCALE);
}


/* The core beside the executable wins, so a moved or copied release folder still runs. */
static char *find_core_beside_exe(void)
{
   const char *base = SDL_GetBasePath();
   char *path = NULL;
   SDL_PathInfo info;

   if (base && SDL_asprintf(&path, "%s%s", base, CORE_FILE_NAME) >= 0 && SDL_GetPathInfo(path, &info))
      return path;
   SDL_free(path);
   return NULL;
}

static bool parse_long(const char *text, long min, long max, long *value)
{
   char *end = NULL;
   long v = strtol(text, &end, 10);

   if (end == text || *end != '\0' || v < min || v > max)
      return false;

   *value = v;
   return true;
}

int main(int argc, char *argv[])
{
   settings_load(&settings);

   char *beside_core = find_core_beside_exe();
   struct options opt = {
      .core_path = beside_core ? beside_core : DEFAULT_CORE,
      .system_dir = settings.system_dir,
      .rom = NULL,
      .scale = settings.scale,
      .max_frames = -1,
      .fullscreen = false,
      .bankswitch = false,
      .xbrz = -1,
   };
   int i;
   int status;

   for (i = 1; i < argc; i++)
   {
      const char *arg = argv[i];
      bool has_value = i + 1 < argc;

      if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
      {
         usage(argv[0]);
         return 0;
      }
      else if ((!strcmp(arg, "-c") || !strcmp(arg, "--core")) && has_value)
         opt.core_path = argv[++i];
      else if ((!strcmp(arg, "-s") || !strcmp(arg, "--system")) && has_value)
         opt.system_dir = argv[++i];
      else if ((!strcmp(arg, "-w") || !strcmp(arg, "--scale")) && has_value)
      {
         if (!parse_long(argv[++i], 1, MAX_SCALE, &opt.scale))
         {
            usage(argv[0]);
            return 2;
         }
      }
      else if (!strcmp(arg, "-f") || !strcmp(arg, "--fullscreen"))
         opt.fullscreen = true;
      else if (!strcmp(arg, "-x") || !strcmp(arg, "--xbrz"))
         opt.xbrz = 1;
      else if (!strcmp(arg, "--no-xbrz"))
         opt.xbrz = 0;
      else if (!strcmp(arg, "-b") || !strcmp(arg, "--bankswitch"))
         opt.bankswitch = true;
      else if ((!strcmp(arg, "-d") || !strcmp(arg, "--frames")) && has_value)
      {
         if (!parse_long(argv[++i], 1, LONG_MAX, &opt.max_frames))
         {
            usage(argv[0]);
            return 2;
         }
      }
      else if (arg[0] != '-' && !opt.rom)
         opt.rom = arg;
      else
      {
         fprintf(stderr, "unknown or incomplete argument: %s\n", arg);
         usage(argv[0]);
         return 2;
      }
   }

   if (opt.max_frames >= 0 && !opt.rom)
   {
      fprintf(stderr, "--frames needs a GAME to run\n");
      usage(argv[0]);
      return 2;
   }

   if (opt.bankswitch && !opt.rom)
   {
      fprintf(stderr, "--bankswitch needs a GAME to load\n");
      usage(argv[0]);
      return 2;
   }

   bios_find(opt.system_dir, &bios);
   status = run(&opt);
   bios_free(&bios);
   settings_free(&settings);
   SDL_free(beside_core);
   return status;
}
