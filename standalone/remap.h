#ifndef INTELLIEMU_REMAP_H
#define INTELLIEMU_REMAP_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "controls.h"

/* Edits keys in place until the screen closes. The caller must keep keys alive while it is open. */
void remap_open(SDL_Scancode keys[CONTROL_COUNT]);
bool remap_is_open(void);

/* Returns true if the key map changed and should be applied and saved. */
bool remap_handle_key(const SDL_KeyboardEvent *key);

/* Draws over the picture area, which starts below the menu bar. */
void remap_render(SDL_Renderer *renderer, int width, int height);

#endif
