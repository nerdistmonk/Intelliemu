#include <string.h>

#include "intelliemu.h"

#define PORT_COUNT 2
#define KEYPAD_MAX 32
#define TRIGGER_THRESHOLD 8000

struct key_binding
{
   SDL_Scancode key;
   unsigned id;
};

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

static const struct key_binding key_bindings[] = {
   { SDL_SCANCODE_UP,        RETRO_DEVICE_ID_JOYPAD_UP },
   { SDL_SCANCODE_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN },
   { SDL_SCANCODE_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT },
   { SDL_SCANCODE_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT },
   { SDL_SCANCODE_Z,         RETRO_DEVICE_ID_JOYPAD_A },
   { SDL_SCANCODE_X,         RETRO_DEVICE_ID_JOYPAD_B },
   { SDL_SCANCODE_C,         RETRO_DEVICE_ID_JOYPAD_Y },
   { SDL_SCANCODE_V,         RETRO_DEVICE_ID_JOYPAD_X },
   { SDL_SCANCODE_RETURN,    RETRO_DEVICE_ID_JOYPAD_START },
   { SDL_SCANCODE_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_SELECT },
   { SDL_SCANCODE_Q,         RETRO_DEVICE_ID_JOYPAD_L },
   { SDL_SCANCODE_E,         RETRO_DEVICE_ID_JOYPAD_R },
};

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

static uint32_t keypad_character(SDL_Scancode sc)
{
   if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
      return '1' + (sc - SDL_SCANCODE_1);
   if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9)
      return '1' + (sc - SDL_SCANCODE_KP_1);
   if (sc == SDL_SCANCODE_0 || sc == SDL_SCANCODE_KP_0)
      return '0';

   switch (sc)
   {
      case SDL_SCANCODE_LEFTBRACKET:
         return '[';
      case SDL_SCANCODE_RIGHTBRACKET:
      case SDL_SCANCODE_KP_ENTER:
         return ']';
      default:
         return 0;
   }
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

static void latch_key(SDL_Scancode sc)
{
   size_t i;

   for (i = 0; i < SDL_arraysize(key_bindings); i++)
      if (key_bindings[i].key == sc)
         latched[0] |= (uint16_t)(1u << key_bindings[i].id);
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

void input_init(void)
{
   int count = 0;
   SDL_JoystickID *ids = SDL_GetGamepads(&count);
   int i;

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
            uint32_t character = keypad_character(event->key.scancode);

            if (character)
               keypad_press(event->key.scancode, character);
            latch_key(event->key.scancode);
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
   for (i = 0; i < SDL_arraysize(key_bindings); i++)
      if (keys[key_bindings[i].key])
         keyboard_bits |= (uint16_t)(1u << key_bindings[i].id);

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
