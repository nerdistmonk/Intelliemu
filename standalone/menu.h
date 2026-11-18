#ifndef INTELLIEMU_MENU_H
#define INTELLIEMU_MENU_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "recent.h"

#define MENU_BAR_HEIGHT 12

enum menu_command
{
   MENU_NONE,
   MENU_OPEN_ROM,
   MENU_CLOSE_ROM,
   MENU_RECENT,
   MENU_EXIT,
   MENU_RESET,
   MENU_TOGGLE_PAUSE,
   MENU_SAVE_STATE,
   MENU_LOAD_STATE,
   MENU_NEXT_SLOT,
   MENU_TOGGLE_FULLSCREEN,
   MENU_TOGGLE_MUTE,
};

struct menu_state
{
   bool game_running;
   bool paused;
   bool fullscreen;
   bool muted;
   int state_slot;
   int canvas_width;
   const struct recent_list *recent;
};

struct menu_action
{
   enum menu_command command;
   /* Index into the recent list for MENU_RECENT. */
   int arg;
};

bool menu_is_open(void);
void menu_close(void);

/* Coordinates are logical canvas pixels. Returns true if the click was consumed by the menu. */
bool menu_click(const struct menu_state *state, float x, float y, struct menu_action *action);
void menu_hover(float x, float y);
void menu_render(SDL_Renderer *renderer, const struct menu_state *state);

#endif
