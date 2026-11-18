#include <string.h>

#include "controls.h"

static const struct
{
   const char *name;
   const char *label;
} control_info[CONTROL_COUNT] = {
   [CONTROL_DISC_UP]       = { "disc_up",       "Disc up" },
   [CONTROL_DISC_DOWN]     = { "disc_down",     "Disc down" },
   [CONTROL_DISC_LEFT]     = { "disc_left",     "Disc left" },
   [CONTROL_DISC_RIGHT]    = { "disc_right",    "Disc right" },
   [CONTROL_LEFT_ACTION]   = { "left_action",   "Left action" },
   [CONTROL_RIGHT_ACTION]  = { "right_action",  "Right action" },
   [CONTROL_TOP_ACTION]    = { "top_action",    "Top action" },
   [CONTROL_BOTTOM_ACTION] = { "bottom_action", "Bottom action" },
   [CONTROL_START]         = { "start",         "Start" },
   [CONTROL_SELECT]        = { "select",        "Select" },
   [CONTROL_L_BUTTON]      = { "left_side",     "L button" },
   [CONTROL_R_BUTTON]      = { "right_side",    "R button" },
   [CONTROL_KEYPAD_1]      = { "keypad_1",      "Keypad 1" },
   [CONTROL_KEYPAD_2]      = { "keypad_2",      "Keypad 2" },
   [CONTROL_KEYPAD_3]      = { "keypad_3",      "Keypad 3" },
   [CONTROL_KEYPAD_4]      = { "keypad_4",      "Keypad 4" },
   [CONTROL_KEYPAD_5]      = { "keypad_5",      "Keypad 5" },
   [CONTROL_KEYPAD_6]      = { "keypad_6",      "Keypad 6" },
   [CONTROL_KEYPAD_7]      = { "keypad_7",      "Keypad 7" },
   [CONTROL_KEYPAD_8]      = { "keypad_8",      "Keypad 8" },
   [CONTROL_KEYPAD_9]      = { "keypad_9",      "Keypad 9" },
   [CONTROL_KEYPAD_0]      = { "keypad_0",      "Keypad 0" },
   [CONTROL_KEYPAD_CLEAR]  = { "keypad_clear",  "Clear" },
   [CONTROL_KEYPAD_ENTER]  = { "keypad_enter",  "Enter" },
};

/* Player 1 uses the number row and WASD; player 2 uses the arrows, Insert/Home block, and numpad. */
static const SDL_Scancode default_keys[PLAYER_COUNT][CONTROL_COUNT] = {
   {
      [CONTROL_DISC_UP]       = SDL_SCANCODE_W,
      [CONTROL_DISC_DOWN]     = SDL_SCANCODE_S,
      [CONTROL_DISC_LEFT]     = SDL_SCANCODE_A,
      [CONTROL_DISC_RIGHT]    = SDL_SCANCODE_D,
      [CONTROL_LEFT_ACTION]   = SDL_SCANCODE_Q,
      [CONTROL_RIGHT_ACTION]  = SDL_SCANCODE_E,
      [CONTROL_TOP_ACTION]    = SDL_SCANCODE_TAB,
      [CONTROL_BOTTOM_ACTION] = SDL_SCANCODE_R,
      [CONTROL_START]         = SDL_SCANCODE_Z,
      [CONTROL_SELECT]        = SDL_SCANCODE_X,
      [CONTROL_L_BUTTON]      = SDL_SCANCODE_RETURN,
      [CONTROL_R_BUTTON]      = SDL_SCANCODE_BACKSPACE,
      [CONTROL_KEYPAD_1]      = SDL_SCANCODE_1,
      [CONTROL_KEYPAD_2]      = SDL_SCANCODE_2,
      [CONTROL_KEYPAD_3]      = SDL_SCANCODE_3,
      [CONTROL_KEYPAD_4]      = SDL_SCANCODE_4,
      [CONTROL_KEYPAD_5]      = SDL_SCANCODE_5,
      [CONTROL_KEYPAD_6]      = SDL_SCANCODE_6,
      [CONTROL_KEYPAD_7]      = SDL_SCANCODE_7,
      [CONTROL_KEYPAD_8]      = SDL_SCANCODE_8,
      [CONTROL_KEYPAD_9]      = SDL_SCANCODE_9,
      [CONTROL_KEYPAD_0]      = SDL_SCANCODE_0,
      [CONTROL_KEYPAD_CLEAR]  = SDL_SCANCODE_C,
      [CONTROL_KEYPAD_ENTER]  = SDL_SCANCODE_V,
   },
   {
      [CONTROL_DISC_UP]       = SDL_SCANCODE_UP,
      [CONTROL_DISC_DOWN]     = SDL_SCANCODE_DOWN,
      [CONTROL_DISC_LEFT]     = SDL_SCANCODE_LEFT,
      [CONTROL_DISC_RIGHT]    = SDL_SCANCODE_RIGHT,
      [CONTROL_LEFT_ACTION]   = SDL_SCANCODE_DELETE,
      [CONTROL_RIGHT_ACTION]  = SDL_SCANCODE_PAGEDOWN,
      [CONTROL_TOP_ACTION]    = SDL_SCANCODE_HOME,
      [CONTROL_BOTTOM_ACTION] = SDL_SCANCODE_END,
      [CONTROL_START]         = SDL_SCANCODE_KP_DIVIDE,
      [CONTROL_SELECT]        = SDL_SCANCODE_KP_MULTIPLY,
      [CONTROL_L_BUTTON]      = SDL_SCANCODE_INSERT,
      [CONTROL_R_BUTTON]      = SDL_SCANCODE_PAGEUP,
      [CONTROL_KEYPAD_1]      = SDL_SCANCODE_KP_1,
      [CONTROL_KEYPAD_2]      = SDL_SCANCODE_KP_2,
      [CONTROL_KEYPAD_3]      = SDL_SCANCODE_KP_3,
      [CONTROL_KEYPAD_4]      = SDL_SCANCODE_KP_4,
      [CONTROL_KEYPAD_5]      = SDL_SCANCODE_KP_5,
      [CONTROL_KEYPAD_6]      = SDL_SCANCODE_KP_6,
      [CONTROL_KEYPAD_7]      = SDL_SCANCODE_KP_7,
      [CONTROL_KEYPAD_8]      = SDL_SCANCODE_KP_8,
      [CONTROL_KEYPAD_9]      = SDL_SCANCODE_KP_9,
      [CONTROL_KEYPAD_0]      = SDL_SCANCODE_KP_0,
      [CONTROL_KEYPAD_CLEAR]  = SDL_SCANCODE_KP_PERIOD,
      [CONTROL_KEYPAD_ENTER]  = SDL_SCANCODE_KP_ENTER,
   },
};

const char *control_name(enum control c)
{
   return control_info[c].name;
}

const char *control_label(enum control c)
{
   return control_info[c].label;
}

SDL_Scancode control_default_key(int player, enum control c)
{
   return default_keys[player][c];
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

const char *pad_name(int input)
{
   if (input == PAD_LEFT_TRIGGER)
      return "lefttrigger";
   if (input == PAD_RIGHT_TRIGGER)
      return "righttrigger";
   if (input < 0 || input >= SDL_GAMEPAD_BUTTON_COUNT)
      return NULL;
   return SDL_GetGamepadStringForButton((SDL_GamepadButton)input);
}

int pad_find(const char *name)
{
   SDL_GamepadButton button;

   if (strcmp(name, "lefttrigger") == 0)
      return PAD_LEFT_TRIGGER;
   if (strcmp(name, "righttrigger") == 0)
      return PAD_RIGHT_TRIGGER;

   button = SDL_GetGamepadButtonFromString(name);
   if (button == SDL_GAMEPAD_BUTTON_INVALID)
      return PAD_UNBOUND;
   return (int)button;
}

void controls_reset(struct control_map maps[PLAYER_COUNT])
{
   for (int p = 0; p < PLAYER_COUNT; p++)
   {
      for (int i = 0; i < CONTROL_COUNT; i++)
      {
         maps[p].keys[i] = default_keys[p][i];
         maps[p].pads[i] = PAD_UNBOUND;
      }
   }
}

int controls_bind_key(struct control_map *map, enum control c, SDL_Scancode key)
{
   int displaced = -1;

   if (key != SDL_SCANCODE_UNKNOWN)
   {
      for (int i = 0; i < CONTROL_COUNT; i++)
      {
         if (i != (int)c && map->keys[i] == key)
         {
            displaced = i;
            break;
         }
      }
   }

   if (displaced >= 0)
      map->keys[displaced] = map->keys[c];
   map->keys[c] = key;
   return displaced;
}

int controls_bind_pad(struct control_map *map, enum control c, int input)
{
   int displaced = -1;

   if (input != PAD_UNBOUND)
   {
      for (int i = 0; i < CONTROL_COUNT; i++)
      {
         if (i != (int)c && map->pads[i] == input)
         {
            displaced = i;
            break;
         }
      }
   }

   if (displaced >= 0)
      map->pads[displaced] = map->pads[c];
   map->pads[c] = input;
   return displaced;
}

void controls_dedupe(struct control_map *map)
{
   for (int i = 0; i < CONTROL_COUNT; i++)
   {
      for (int j = i + 1; j < CONTROL_COUNT; j++)
      {
         if (map->keys[i] != SDL_SCANCODE_UNKNOWN && map->keys[j] == map->keys[i])
            map->keys[j] = SDL_SCANCODE_UNKNOWN;
         if (map->pads[i] != PAD_UNBOUND && map->pads[j] == map->pads[i])
            map->pads[j] = PAD_UNBOUND;
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
