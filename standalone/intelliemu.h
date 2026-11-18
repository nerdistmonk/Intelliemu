#ifndef INTELLIEMU_H
#define INTELLIEMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "libretro.h"

typedef struct core
{
   SDL_SharedObject *handle;
   const char *system_dir;
   struct retro_keyboard_callback keyboard;
   bool initialized;
   bool game_loaded;

   unsigned (RETRO_CALLCONV *api_version)(void);
   void (RETRO_CALLCONV *get_system_av_info)(struct retro_system_av_info *info);
   void (RETRO_CALLCONV *set_environment)(retro_environment_t cb);
   void (RETRO_CALLCONV *set_video_refresh)(retro_video_refresh_t cb);
   void (RETRO_CALLCONV *set_audio_sample)(retro_audio_sample_t cb);
   void (RETRO_CALLCONV *set_audio_sample_batch)(retro_audio_sample_batch_t cb);
   void (RETRO_CALLCONV *set_input_poll)(retro_input_poll_t cb);
   void (RETRO_CALLCONV *set_input_state)(retro_input_state_t cb);
   void (RETRO_CALLCONV *init)(void);
   void (RETRO_CALLCONV *deinit)(void);
   bool (RETRO_CALLCONV *load_game)(const struct retro_game_info *game);
   void (RETRO_CALLCONV *unload_game)(void);
   void (RETRO_CALLCONV *run)(void);
   void (RETRO_CALLCONV *reset)(void);
   size_t (RETRO_CALLCONV *serialize_size)(void);
   bool (RETRO_CALLCONV *serialize)(void *data, size_t size);
   bool (RETRO_CALLCONV *unserialize)(const void *data, size_t size);
} core_t;

bool core_open(core_t *core, const char *library, const char *system_dir);
bool core_load_game(core_t *core, const char *rom_path);
void core_close(core_t *core);
void core_keyboard_event(bool down, uint32_t character);

void input_init(void);
void input_shutdown(void);
void input_handle_event(const SDL_Event *event);
void input_release_all(void);
void input_after_run(void);

/* Implemented by main.c and input.c; called by the core. */
void frontend_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch);
void frontend_audio_sample(int16_t left, int16_t right);
size_t frontend_audio_sample_batch(const int16_t *data, size_t frames);
void frontend_input_poll(void);
int16_t frontend_input_state(unsigned port, unsigned device, unsigned index, unsigned id);

#endif
