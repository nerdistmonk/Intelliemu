#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_main.h>

#include "intelliemu.h"

#ifdef _WIN32
#define DEFAULT_CORE "./freeintv_libretro.dll"
#else
#define DEFAULT_CORE "./freeintv_libretro.so"
#endif

#define DEFAULT_SCALE 3
#define MAX_SCALE 16
#define MAX_STATE_SLOT 9
#define STAGE_FRAMES 4096
#define AUDIO_LEAD_FRAMES 3

struct options
{
   const char *core_path;
   const char *system_dir;
   const char *rom;
   long scale;
   long max_frames;
};

static core_t core;
static const char *rom_file;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static unsigned texture_w, texture_h;
static SDL_AudioStream *audio;
static int audio_lead_bytes;
static uint64_t frame_ns;
static uint64_t next_frame_ns;
static int state_slot = 1;
static bool quit_requested;
static int16_t stage[STAGE_FRAMES * 2];
static size_t stage_frames;
static bool audio_error_reported;

void frontend_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
   /* A NULL frame means the core wants the previous frame shown again. */
   if (!data)
      return;

   if (!texture || texture_w != width || texture_h != height)
   {
      if (texture)
         SDL_DestroyTexture(texture);

      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, (int)width, (int)height);
      if (!texture)
      {
         fprintf(stderr, "cannot create video texture: %s\n", SDL_GetError());
         return;
      }

      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
      SDL_SetRenderLogicalPresentation(renderer, (int)width, (int)height,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
      texture_w = width;
      texture_h = height;
   }

   SDL_UpdateTexture(texture, NULL, data, (int)pitch);
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

static void audio_open(int sample_rate)
{
   SDL_AudioSpec spec = { .format = SDL_AUDIO_S16, .channels = 2, .freq = sample_rate };

   if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
      fprintf(stderr, "audio disabled, pacing by timer: %s\n", SDL_GetError());
      return;
   }

   audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
   if (!audio)
   {
      fprintf(stderr, "audio disabled, pacing by timer: %s\n", SDL_GetError());
      return;
   }

   SDL_ResumeAudioStreamDevice(audio);
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

static void render(void)
{
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderClear(renderer);

   if (texture)
      SDL_RenderTexture(renderer, texture, NULL, NULL);

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
   size_t size = core.serialize_size();
   void *buf = SDL_malloc(size);
   char *path = state_path(state_slot);

   if (buf && path && core.serialize(buf, size) && write_file(path, buf, size))
      fprintf(stderr, "saved state slot %d to %s\n", state_slot, path);
   else
      fprintf(stderr, "failed to save state slot %d\n", state_slot);

   SDL_free(buf);
   SDL_free(path);
}

static void load_state(void)
{
   size_t size = core.serialize_size();
   void *buf = SDL_malloc(size);
   char *path = state_path(state_slot);

   if (buf && path && read_file(path, buf, size) && core.unserialize(buf, size))
   {
      if (audio)
         SDL_ClearAudioStream(audio);
      fprintf(stderr, "loaded state slot %d from %s\n", state_slot, path);
   }
   else
   {
      fprintf(stderr, "failed to load state slot %d\n", state_slot);
   }

   SDL_free(buf);
   SDL_free(path);
}

static void toggle_fullscreen(void)
{
   SDL_WindowFlags flags = SDL_GetWindowFlags(window);

   SDL_SetWindowFullscreen(window, !(flags & SDL_WINDOW_FULLSCREEN));
}

static void handle_hotkey(SDL_Scancode key)
{
   switch (key)
   {
      case SDL_SCANCODE_ESCAPE:
         quit_requested = true;
         break;
      case SDL_SCANCODE_F1:
         save_state();
         break;
      case SDL_SCANCODE_F5:
         load_state();
         break;
      case SDL_SCANCODE_F9:
         state_slot = state_slot % MAX_STATE_SLOT + 1;
         fprintf(stderr, "state slot %d\n", state_slot);
         break;
      case SDL_SCANCODE_F10:
         core.reset();
         break;
      case SDL_SCANCODE_F11:
         toggle_fullscreen();
         break;
      default:
         break;
   }
}

static void main_loop(long max_frames)
{
   long frames_run = 0;

   next_frame_ns = SDL_GetTicksNS();

   while (!quit_requested)
   {
      SDL_Event event;

      while (SDL_PollEvent(&event))
      {
         if (event.type == SDL_EVENT_QUIT)
            quit_requested = true;
         else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
            handle_hotkey(event.key.scancode);

         input_handle_event(&event);
      }

      if (frame_due())
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

static void warn_missing_bios(const char *dir)
{
   static const char *const files[] = { "exec.bin", "grom.bin" };
   size_t i;

   for (i = 0; i < sizeof(files) / sizeof(files[0]); i++)
   {
      char *path = NULL;
      SDL_PathInfo info;

      if (SDL_asprintf(&path, "%s/%s", dir, files[i]) >= 0 &&
            !(SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE))
         fprintf(stderr, "warning: %s not found; the core needs both BIOS images\n", path);

      SDL_free(path);
   }
}

static int run(const struct options *opt)
{
   struct retro_system_av_info av;
   int status = 1;

   rom_file = opt->rom;

   if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
   {
      fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return 1;
   }

   input_init();

   if (!core_open(&core, opt->core_path, opt->system_dir) || !core_load_game(&core, opt->rom))
      goto cleanup;

   core.get_system_av_info(&av);
   frame_ns = (uint64_t)(1000000000.0 / av.timing.fps);
   audio_lead_bytes = (int)(av.timing.sample_rate / av.timing.fps * AUDIO_LEAD_FRAMES) * 2 * (int)sizeof(int16_t);

   window = SDL_CreateWindow("Intelliemu",
         (int)(av.geometry.base_width * (unsigned)opt->scale),
         (int)(av.geometry.base_height * (unsigned)opt->scale),
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

   SDL_SetRenderVSync(renderer, 1);
   audio_open((int)av.timing.sample_rate);

   main_loop(opt->max_frames);
   status = 0;

cleanup:
   input_release_all();
   input_shutdown();
   if (audio)
      SDL_DestroyAudioStream(audio);
   if (texture)
      SDL_DestroyTexture(texture);
   if (renderer)
      SDL_DestroyRenderer(renderer);
   if (window)
      SDL_DestroyWindow(window);
   core_close(&core);
   SDL_Quit();
   return status;
}

static void usage(const char *prog)
{
   fprintf(stderr,
         "usage: %s [options] GAME\n"
         "  -c, --core PATH    libretro core library (default: %s)\n"
         "  -s, --system DIR   directory with exec.bin and grom.bin (default: .)\n"
         "  -x, --scale N      initial window scale, 1-%d (default: %d)\n"
         "  -f, --frames N     exit after N frames (for testing)\n"
         "  -h, --help         show this help\n"
         "\n"
         "hotkeys: F1 save state, F5 load state, F9 next slot, F10 reset,\n"
         "         F11 fullscreen, Esc quit\n"
         "keyboard (player 1): arrows = D-pad, Z/X/C/V = A/B/Y/X, Q/E = L/R,\n"
         "         Return = start, Backspace = select, 0-9 / [ / ] = keypad\n",
         prog, DEFAULT_CORE, MAX_SCALE, DEFAULT_SCALE);
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
   struct options opt = {
      .core_path = DEFAULT_CORE,
      .system_dir = ".",
      .rom = NULL,
      .scale = DEFAULT_SCALE,
      .max_frames = -1,
   };
   int i;

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
      else if ((!strcmp(arg, "-x") || !strcmp(arg, "--scale")) && has_value)
      {
         if (!parse_long(argv[++i], 1, MAX_SCALE, &opt.scale))
         {
            usage(argv[0]);
            return 2;
         }
      }
      else if ((!strcmp(arg, "-f") || !strcmp(arg, "--frames")) && has_value)
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

   if (!opt.rom)
   {
      usage(argv[0]);
      return 2;
   }

   warn_missing_bios(opt.system_dir);
   return run(&opt);
}
