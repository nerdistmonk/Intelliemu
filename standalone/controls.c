#include <string.h>

#include "controls.h"

static const struct
{
   const char *name;
   const char *label;
   SDL_Scancode def;
} control_info[CONTROL_COUNT] = {
   [CONTROL_DISC_UP]       = { "disc_up",       "Disc up",     SDL_SCANCODE_UP },
   [CONTROL_DISC_DOWN]     = { "disc_down",     "Disc down",   SDL_SCANCODE_DOWN },
   [CONTROL_DISC_LEFT]     = { "disc_left",     "Disc left",   SDL_SCANCODE_LEFT },
   [CONTROL_DISC_RIGHT]    = { "disc_right",    "Disc right",  SDL_SCANCODE_RIGHT },
   [CONTROL_LEFT_ACTION]   = { "left_action",   "Left action", SDL_SCANCODE_Z },
   [CONTROL_RIGHT_ACTION]  = { "right_action", "Right action", SDL_SCANCODE_X },
   [CONTROL_TOP_ACTION]    = { "top_action",    "Top action",  SDL_SCANCODE_C },
   [CONTROL_BOTTOM_ACTION] = { "bottom_action", "Bottom action", SDL_SCANCODE_V },
   [CONTROL_START]         = { "start",         "Start",       SDL_SCANCODE_RETURN },
   [CONTROL_SELECT]        = { "select",        "Select",      SDL_SCANCODE_BACKSPACE },
   [CONTROL_L_BUTTON]      = { "left_side",     "L button",    SDL_SCANCODE_Q },
   [CONTROL_R_BUTTON]      = { "right_side",    "R button",    SDL_SCANCODE_E },
   [CONTROL_KEYPAD_1]      = { "keypad_1",      "Keypad 1",    SDL_SCANCODE_1 },
   [CONTROL_KEYPAD_2]      = { "keypad_2",      "Keypad 2",    SDL_SCANCODE_2 },
   [CONTROL_KEYPAD_3]      = { "keypad_3",      "Keypad 3",    SDL_SCANCODE_3 },
   [CONTROL_KEYPAD_4]      = { "keypad_4",      "Keypad 4",    SDL_SCANCODE_4 },
   [CONTROL_KEYPAD_5]      = { "keypad_5",      "Keypad 5",    SDL_SCANCODE_5 },
   [CONTROL_KEYPAD_6]      = { "keypad_6",      "Keypad 6",    SDL_SCANCODE_6 },
   [CONTROL_KEYPAD_7]      = { "keypad_7",      "Keypad 7",    SDL_SCANCODE_7 },
   [CONTROL_KEYPAD_8]      = { "keypad_8",      "Keypad 8",    SDL_SCANCODE_8 },
   [CONTROL_KEYPAD_9]      = { "keypad_9",      "Keypad 9",    SDL_SCANCODE_9 },
   [CONTROL_KEYPAD_0]      = { "keypad_0",      "Keypad 0",    SDL_SCANCODE_0 },
   [CONTROL_KEYPAD_CLEAR]  = { "keypad_clear",  "Clear",       SDL_SCANCODE_LEFTBRACKET },
   [CONTROL_KEYPAD_ENTER]  = { "keypad_enter",  "Enter",       SDL_SCANCODE_RIGHTBRACKET },
};

const char *control_name(enum control c)
{
   return control_info[c].name;
}

const char *control_label(enum control c)
{
   return control_info[c].label;
}

SDL_Scancode control_default(enum control c)
{
   return control_info[c].def;
}

int control_find(const char *name)
{
   for (int i = 0; i < CONTROL_COUNT; i++)
   {
      if (strcmp(control_info[i].name, name) == 0)
         return i;
   }
   return -1;
}

void controls_reset(SDL_Scancode keys[CONTROL_COUNT])
{
   for (int i = 0; i < CONTROL_COUNT; i++)
      keys[i] = control_info[i].def;
}

int controls_bind(SDL_Scancode keys[CONTROL_COUNT], enum control c, SDL_Scancode sc)
{
   int displaced = -1;

   if (sc != SDL_SCANCODE_UNKNOWN)
   {
      for (int i = 0; i < CONTROL_COUNT; i++)
      {
         if (i != (int)c && keys[i] == sc)
         {
            displaced = i;
            break;
         }
      }
   }

   if (displaced >= 0)
      keys[displaced] = keys[c];
   keys[c] = sc;
   return displaced;
}

void controls_dedupe(SDL_Scancode keys[CONTROL_COUNT])
{
   for (int i = 0; i < CONTROL_COUNT; i++)
   {
      if (keys[i] == SDL_SCANCODE_UNKNOWN)
         continue;
      for (int j = i + 1; j < CONTROL_COUNT; j++)
      {
         if (keys[j] == keys[i])
            keys[j] = SDL_SCANCODE_UNKNOWN;
      }
   }
}

/* Mirrors hotkey_command() in main.c. */
bool controls_is_reserved(SDL_Scancode sc, SDL_Keymod mod)
{
   if (sc == SDL_SCANCODE_ESCAPE)
      return true;

   if (mod & SDL_KMOD_CTRL)
   {
      if (sc == SDL_SCANCODE_O || sc == SDL_SCANCODE_W || sc == SDL_SCANCODE_Q)
         return true;
   }

   switch (sc)
   {
      case SDL_SCANCODE_F1:
      case SDL_SCANCODE_F5:
      case SDL_SCANCODE_F9:
      case SDL_SCANCODE_F10:
      case SDL_SCANCODE_F11:
         return true;
      default:
         return false;
   }
}
