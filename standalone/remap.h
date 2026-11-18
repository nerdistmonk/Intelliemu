#ifndef INTELLIEMU_REMAP_H
#define INTELLIEMU_REMAP_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "controls.h"

enum remap_device
{
   REMAP_KEYBOARD,
   REMAP_GAMEPAD
};

/* Edits maps[player] in place until the screen closes. The caller must keep maps alive while it is open. */
void remap_open(struct control_map maps[PLAYER_COUNT], int player, enum remap_device device);
bool remap_is_open(void);

/* True while waiting for a gamepad input. The caller should then pass pad input to remap_handle_pad. */
bool remap_wants_pad(void);

/* Return true if the bindings changed and should be applied and saved. */
bool remap_handle_key(const SDL_KeyboardEvent *key);
bool remap_handle_pad(int input);

/* Draws over the picture area, which starts below the menu bar. pad_names[p] is NULL when player p has no gamepad. */
void remap_render(SDL_Renderer *renderer, int width, int height, const char *const pad_names[PLAYER_COUNT], bool p1_left);

#endif
