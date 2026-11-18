#include <stdio.h>
#include <string.h>

#include "bios.h"
#include "intelliemu.h"
#include "../src/intelliemu_env.h"

static core_t *g_core;

static bool RETRO_CALLCONV environment_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         return *(const enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char **)data = g_core->system_dir;
         return true;

      case INTELLIEMU_ENV_BIOS_PATHS:
      {
         struct intelliemu_bios_paths *paths = data;

         paths->exec_path = g_core->bios ? g_core->bios->exec_path : NULL;
         paths->grom_path = g_core->bios ? g_core->bios->grom_path : NULL;
         paths->ecs_path = g_core->bios ? g_core->bios->ecs_path : NULL;
         return true;
      }

      default:
         return false;
   }
}

static void RETRO_CALLCONV video_cb(const void *data, unsigned width, unsigned height, size_t pitch)
{
   frontend_video_refresh(data, width, height, pitch);
}

static void RETRO_CALLCONV audio_sample_cb(int16_t left, int16_t right)
{
   frontend_audio_sample(left, right);
}

static size_t RETRO_CALLCONV audio_sample_batch_cb(const int16_t *data, size_t frames)
{
   return frontend_audio_sample_batch(data, frames);
}

static void RETRO_CALLCONV input_poll_cb(void)
{
   frontend_input_poll();
}

static int16_t RETRO_CALLCONV input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
   return frontend_input_state(port, device, index, id);
}

static bool load_symbol(SDL_SharedObject *handle, const char *name, void *dst)
{
   SDL_FunctionPointer fn = SDL_LoadFunction(handle, name);

   if (!fn)
   {
      fprintf(stderr, "core is missing symbol '%s': %s\n", name, SDL_GetError());
      return false;
   }

   /* Copy the pointer bytes so no incompatible function-pointer cast is needed. */
   memcpy(dst, &fn, sizeof(fn));
   return true;
}

#define LOAD_SYMBOL(field, name) \
   do { if (!load_symbol(core->handle, (name), &core->field)) goto fail; } while (0)

bool core_open(core_t *core, const char *library, const char *system_dir, const struct bios_files *bios)
{
   char *local_path = NULL;
   const char *load_path = library;

   memset(core, 0, sizeof(*core));
   core->system_dir = system_dir;
   core->bios = bios;
   g_core = core;

   /* SDL_LoadObject does not search the working directory for bare names. */
   if (!strchr(library, '/') && !strchr(library, '\\'))
   {
      if (SDL_asprintf(&local_path, "./%s", library) < 0)
      {
         g_core = NULL;
         return false;
      }
      load_path = local_path;
   }

   core->handle = SDL_LoadObject(load_path);
   SDL_free(local_path);

   if (!core->handle)
   {
      fprintf(stderr, "failed to load core '%s': %s\n", library, SDL_GetError());
      goto fail;
   }

   LOAD_SYMBOL(api_version, "retro_api_version");
   LOAD_SYMBOL(get_system_av_info, "retro_get_system_av_info");
   LOAD_SYMBOL(set_environment, "retro_set_environment");
   LOAD_SYMBOL(set_video_refresh, "retro_set_video_refresh");
   LOAD_SYMBOL(set_audio_sample, "retro_set_audio_sample");
   LOAD_SYMBOL(set_audio_sample_batch, "retro_set_audio_sample_batch");
   LOAD_SYMBOL(set_input_poll, "retro_set_input_poll");
   LOAD_SYMBOL(set_input_state, "retro_set_input_state");
   LOAD_SYMBOL(init, "retro_init");
   LOAD_SYMBOL(deinit, "retro_deinit");
   LOAD_SYMBOL(load_game, "retro_load_game");
   LOAD_SYMBOL(unload_game, "retro_unload_game");
   LOAD_SYMBOL(run, "retro_run");
   LOAD_SYMBOL(reset, "retro_reset");
   LOAD_SYMBOL(serialize_size, "retro_serialize_size");
   LOAD_SYMBOL(serialize, "retro_serialize");
   LOAD_SYMBOL(unserialize, "retro_unserialize");
   LOAD_SYMBOL(keypad_event, "intelliemu_keypad_event");

   if (core->api_version() != RETRO_API_VERSION)
   {
      fprintf(stderr, "core API version %u does not match %u\n", core->api_version(), RETRO_API_VERSION);
      goto fail;
   }

   /* retro_set_environment must come first; all callbacks must be set before retro_init. */
   core->set_environment(environment_cb);
   core->set_video_refresh(video_cb);
   core->set_audio_sample(audio_sample_cb);
   core->set_audio_sample_batch(audio_sample_batch_cb);
   core->set_input_poll(input_poll_cb);
   core->set_input_state(input_state_cb);

   core->init();
   core->initialized = true;
   return true;

fail:
   core_close(core);
   return false;
}

bool core_load_game(core_t *core, const char *rom_path)
{
   SDL_PathInfo info;
   SDL_IOStream *io;
   struct retro_game_info game = { .path = rom_path };

   if (!SDL_GetPathInfo(rom_path, &info))
   {
      fprintf(stderr, "cannot open ROM '%s': %s\n", rom_path, SDL_GetError());
      return false;
   }

   if (info.type != SDL_PATHTYPE_FILE || info.size == 0)
   {
      fprintf(stderr, "ROM '%s' is not a non-empty file\n", rom_path);
      return false;
   }

   io = SDL_IOFromFile(rom_path, "rb");
   if (!io)
   {
      fprintf(stderr, "cannot read ROM '%s': %s\n", rom_path, SDL_GetError());
      return false;
   }
   SDL_CloseIO(io);

   core->game_loaded = core->load_game(&game);
   return core->game_loaded;
}

void core_close(core_t *core)
{
   if (core->game_loaded)
      core->unload_game();

   if (core->initialized)
      core->deinit();

   if (core->handle)
      SDL_UnloadObject(core->handle);

   if (g_core == core)
      g_core = NULL;

   memset(core, 0, sizeof(*core));
}

void core_keypad_event(unsigned port, bool down, uint32_t character)
{
   if (g_core && g_core->keypad_event)
      g_core->keypad_event(port, down, character);
}
