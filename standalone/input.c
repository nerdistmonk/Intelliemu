#include <stdio.h>
#include <string.h>

#include "intelliemu.h"

#define PORT_COUNT 2
#define KEYPAD_MAX 32
#define TRIGGER_THRESHOLD 8000

struct pad_binding
{
   SDL_GamepadButton button;
   unsigned id;
};

struct held_key
{
   SDL_Scancode scancode;
   uint32_t character;
};

/* Indexed by control. The first CONTROL_JOYPAD_COUNT controls are joypad buttons, in controls.h order. */
static const unsigned joypad_ids[CONTROL_JOYPAD_COUNT] = {
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_B,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_R,
};

/* Copied from the config by input_set_key_map(). Unbound controls are SDL_SCANCODE_UNKNOWN. */
static SDL_Scancode key_for[CONTROL_COUNT];

static const struct pad_binding pad_bindings[] = {
   { SDL_GAMEPAD_BUTTON_DPAD_UP,        RETRO_DEVICE_ID_JOYPAD_UP },
   { SDL_GAMEPAD_BUTTON_DPAD_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN },
   { SDL_GAMEPAD_BUTTON_DPAD_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT },
   { SDL_GAMEPAD_BUTTON_DPAD_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT },
   { SDL_GAMEPAD_BUTTON_SOUTH,          RETRO_DEVICE_ID_JOYPAD_B },
   { SDL_GAMEPAD_BUTTON_EAST,           RETRO_DEVICE_ID_JOYPAD_A },
   { SDL_GAMEPAD_BUTTON_NORTH,          RETRO_DEVICE_ID_JOYPAD_X },
   { SDL_GAMEPAD_BUTTON_WEST,           RETRO_DEVICE_ID_JOYPAD_Y },
   { SDL_GAMEPAD_BUTTON_START,          RETRO_DEVICE_ID_JOYPAD_START },
   { SDL_GAMEPAD_BUTTON_BACK,           RETRO_DEVICE_ID_JOYPAD_SELECT },
   { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  RETRO_DEVICE_ID_JOYPAD_L },
   { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, RETRO_DEVICE_ID_JOYPAD_R },
   { SDL_GAMEPAD_BUTTON_LEFT_STICK,     RETRO_DEVICE_ID_JOYPAD_L3 },
   { SDL_GAMEPAD_BUTTON_RIGHT_STICK,    RETRO_DEVICE_ID_JOYPAD_R3 },
};

static SDL_Gamepad *pads[PORT_COUNT];

/* Snapshot taken in frontend_input_poll so each retro_run sees one consistent state. */
static struct
{
   uint16_t buttons[PORT_COUNT];
   int16_t axes[PORT_COUNT][2][2];
} snapshot;

static struct held_key held[KEYPAD_MAX];
static int held_count;

/* What the core was last told about the keypad, so keypad_sync() only sends changes. */
static bool keypad_sent_down;
static uint32_t keypad_sent_char;
/* Set from a keypad press until the core has run, so a release can't hide the press. */
static bool keypad_unseen;

/* Joypad presses that began and ended between two core polls, OR'd into the next poll. */
static uint16_t latched[PORT_COUNT];

/* While blocked (menu open, paused, or no game), the core sees no input. Releases still count. */
static bool blocked;

/* Returns the core keypad character for a control, or 0 if the control is not a keypad button. */
static uint32_t keypad_character(int control)
{
   if (control >= CONTROL_KEYPAD_1 && control <= CONTROL_KEYPAD_9)
      return '1' + (control - CONTROL_KEYPAD_1);

   switch (control)
   {
      case CONTROL_KEYPAD_0:
         return '0';
      case CONTROL_KEYPAD_CLEAR:
         return '[';
      case CONTROL_KEYPAD_ENTER:
         return ']';
      default:
         return 0;
   }
}

/* Numpad keys work as extra keys for their keypad button, but only while that button is bound. */
static int numpad_alias(SDL_Scancode sc)
{
   if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9)
      return CONTROL_KEYPAD_1 + (sc - SDL_SCANCODE_KP_1);
   if (sc == SDL_SCANCODE_KP_0)
      return CONTROL_KEYPAD_0;
   if (sc == SDL_SCANCODE_KP_ENTER)
      return CONTROL_KEYPAD_ENTER;
   return -1;
}

/* An explicit binding wins over a numpad alias. Returns -1 if the key does nothing. */
static int control_for_key(SDL_Scancode sc)
{
   int control;

   if (sc == SDL_SCANCODE_UNKNOWN)
      return -1;

   for (control = 0; control < CONTROL_COUNT; control++)
      if (key_for[control] == sc)
         return control;

   control = numpad_alias(sc);
   if (control >= 0 && key_for[control] != SDL_SCANCODE_UNKNOWN)
      return control;

   return -1;
}

static void keypad_send(bool down, uint32_t character)
{
   keypad_sent_down = down;
   keypad_sent_char = down ? character : 0;
   core_keyboard_event(down, character);
}

/* The core only remembers the last key it was sent, so when the newest key lifts,
 * the key beneath it is sent again. */
static void keypad_sync(void)
{
   bool down = held_count > 0;
   uint32_t character = down ? held[held_count - 1].character : 0;

   if (down == keypad_sent_down && character == keypad_sent_char)
      return;

   keypad_send(down, character);
}

static void keypad_press(SDL_Scancode sc, uint32_t character)
{
   int i;

   if (held_count == KEYPAD_MAX)
      return;

   for (i = 0; i < held_count; i++)
      if (held[i].scancode == sc)
         return;

   held[held_count].scancode = sc;
   held[held_count].character = character;
   held_count++;
   keypad_unseen = true;
   keypad_send(true, character);
}

static void keypad_release(SDL_Scancode sc)
{
   int i;

   for (i = 0; i < held_count; i++)
      if (held[i].scancode == sc)
         break;

   if (i == held_count)
      return;

   held_count--;
   memmove(&held[i], &held[i + 1], (size_t)(held_count - i) * sizeof(held[0]));

   /* Deferred until input_after_run() so the core still sees a press that ended this frame. */
   if (!keypad_unseen)
      keypad_sync();
}

static void latch_control(int control)
{
   if (control >= 0 && control < CONTROL_JOYPAD_COUNT)
      latched[0] |= (uint16_t)(1u << joypad_ids[control]);
}

static void latch_pad_button(SDL_JoystickID id, SDL_GamepadButton button)
{
   size_t i;
   int port;

   for (port = 0; port < PORT_COUNT; port++)
      if (pads[port] && SDL_GetGamepadID(pads[port]) == id)
         break;

   if (port == PORT_COUNT)
      return;

   for (i = 0; i < SDL_arraysize(pad_bindings); i++)
      if (pad_bindings[i].button == button)
         latched[port] |= (uint16_t)(1u << pad_bindings[i].id);
}

static void gamepad_added(SDL_JoystickID id)
{
   int port;

   for (port = 0; port < PORT_COUNT; port++)
      if (pads[port] && SDL_GetGamepadID(pads[port]) == id)
         return;

   for (port = 0; port < PORT_COUNT; port++)
   {
      if (!pads[port])
      {
         pads[port] = SDL_OpenGamepad(id);
         return;
      }
   }
}

static void gamepad_removed(SDL_JoystickID id)
{
   int port;

   for (port = 0; port < PORT_COUNT; port++)
   {
      if (pads[port] && SDL_GetGamepadID(pads[port]) == id)
      {
         SDL_CloseGamepad(pads[port]);
         pads[port] = NULL;
      }
   }
}

static void load_gamepad_mappings(void)
{
   const char *base = SDL_GetBasePath();
   char *path = NULL;

   if (!base || SDL_asprintf(&path, "%sgamecontrollerdb.txt", base) < 0)
      return;

   if (SDL_AddGamepadMappingsFromFile(path) < 0)
      fprintf(stderr, "gamepad mappings not loaded from %s: %s\n", path, SDL_GetError());

   SDL_free(path);
}

void input_set_key_map(const SDL_Scancode keys[CONTROL_COUNT])
{
   memcpy(key_for, keys, sizeof(key_for));
}

void input_init(void)
{
   int count = 0;
   SDL_JoystickID *ids;
   int i;

   load_gamepad_mappings();

   ids = SDL_GetGamepads(&count);
   if (!ids)
      return;

   for (i = 0; i < count; i++)
      gamepad_added(ids[i]);

   SDL_free(ids);
}

void input_shutdown(void)
{
   int port;

   for (port = 0; port < PORT_COUNT; port++)
   {
      if (pads[port])
      {
         SDL_CloseGamepad(pads[port]);
         pads[port] = NULL;
      }
   }

   held_count = 0;
}

void input_release_all(void)
{
   held_count = 0;
   keypad_unseen = false;
   keypad_sync();
   memset(latched, 0, sizeof(latched));
}

void input_after_run(void)
{
   keypad_unseen = false;
   keypad_sync();
}

void input_set_blocked(bool value)
{
   if (value == blocked)
      return;

   blocked = value;
   if (blocked)
      input_release_all();
}

void input_handle_event(const SDL_Event *event)
{
   switch (event->type)
   {
      case SDL_EVENT_KEY_DOWN:
         if (!event->key.repeat && !blocked)
         {
            int control = control_for_key(event->key.scancode);
            uint32_t character = control >= 0 ? keypad_character(control) : 0;

            if (character)
               keypad_press(event->key.scancode, character);
            latch_control(control);
         }
         break;

      case SDL_EVENT_KEY_UP:
         keypad_release(event->key.scancode);
         break;

      case SDL_EVENT_GAMEPAD_ADDED:
         gamepad_added(event->gdevice.which);
         break;

      case SDL_EVENT_GAMEPAD_REMOVED:
         gamepad_removed(event->gdevice.which);
         break;

      case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
         if (!blocked)
            latch_pad_button(event->gbutton.which, (SDL_GamepadButton)event->gbutton.button);
         break;

      case SDL_EVENT_WINDOW_FOCUS_LOST:
         input_release_all();
         break;

      default:
         break;
   }
}

void frontend_input_poll(void)
{
   const bool *keys;
   uint16_t keyboard_bits = 0;
   size_t i;
   int port;

   if (blocked)
   {
      memset(&snapshot, 0, sizeof(snapshot));
      memset(latched, 0, sizeof(latched));
      return;
   }

   keys = SDL_GetKeyboardState(NULL);
   for (i = 0; i < CONTROL_JOYPAD_COUNT; i++)
      if (key_for[i] != SDL_SCANCODE_UNKNOWN && keys[key_for[i]])
         keyboard_bits |= (uint16_t)(1u << joypad_ids[i]);

   for (port = 0; port < PORT_COUNT; port++)
   {
      SDL_Gamepad *pad = pads[port];
      uint16_t bits = (port == 0) ? keyboard_bits : 0;

      memset(snapshot.axes[port], 0, sizeof(snapshot.axes[port]));

      if (pad)
      {
         for (i = 0; i < SDL_arraysize(pad_bindings); i++)
            if (SDL_GetGamepadButton(pad, pad_bindings[i].button))
               bits |= (uint16_t)(1u << pad_bindings[i].id);

         if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > TRIGGER_THRESHOLD)
            bits |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_L2);
         if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > TRIGGER_THRESHOLD)
            bits |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_R2);

         snapshot.axes[port][0][0] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
         snapshot.axes[port][0][1] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
         snapshot.axes[port][1][0] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
         snapshot.axes[port][1][1] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
      }

      bits |= latched[port];
      latched[port] = 0;
      snapshot.buttons[port] = bits;
   }
}

int16_t frontend_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (port >= PORT_COUNT)
      return 0;

   if (device == RETRO_DEVICE_JOYPAD)
      return (id < 16 && ((snapshot.buttons[port] >> id) & 1u)) ? 1 : 0;

   if (device == RETRO_DEVICE_ANALOG && index < 2 && id < 2)
      return snapshot.axes[port][index][id];

   return 0;
}
