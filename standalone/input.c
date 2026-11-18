#include <stdio.h>
#include <string.h>

#include "intelliemu.h"

#define KEYPAD_MAX 32

/* A keypad press that is still held. from_pad says whether code is a gamepad input or a keyboard scancode. */
struct held_key
{
   bool from_pad;
   int code;
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

/* Set by input_set_bindings(). Player N uses the Nth connected gamepad, in connection order. */
/* Set by input_set_bindings(). Player N uses the Nth connected gamepad, in connection order. */
static struct control_map maps[PLAYER_COUNT];

static SDL_Gamepad *pads[PLAYER_COUNT];

/* Snapshot taken in frontend_input_poll so each retro_run sees one consistent state. */
static struct
{
   uint16_t buttons[PLAYER_COUNT];
   int16_t stick[PLAYER_COUNT][2];
} snapshot;

/* Keypad presses still held, per player. The top entry is the one the core is told about. */
static struct held_key held[PLAYER_COUNT][KEYPAD_MAX];
static int held_count[PLAYER_COUNT];

/* What the core was last told about each player's keypad, so keypad_sync() only sends changes. */
static bool keypad_sent_down[PLAYER_COUNT];
static uint32_t keypad_sent_char[PLAYER_COUNT];
/* Set from a keypad press until the core has run, so a release can't hide the press. */
static bool keypad_unseen[PLAYER_COUNT];

/* Joypad presses that began and ended between two core polls, OR'd into the next poll. */
static uint16_t latched[PLAYER_COUNT];

/* Trigger pressed state per player: [0] left, [1] right. Follows the physical trigger even while blocked. */
static bool trigger_down[PLAYER_COUNT][2];

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

static void keypad_send(int port, bool down, uint32_t character)
{
   keypad_sent_down[port] = down;
   keypad_sent_char[port] = down ? character : 0;
   core_keypad_event((unsigned)port, down, character);
}

/* The core only remembers the last key it was sent, so when the newest key lifts,
 * the key beneath it is sent again. */
static void keypad_sync(int port)
{
   bool down = held_count[port] > 0;
   uint32_t character = down ? held[port][held_count[port] - 1].character : 0;

   if (down == keypad_sent_down[port] && character == keypad_sent_char[port])
      return;

   keypad_send(port, down, character);
}

static void keypad_press(int port, bool from_pad, int code, uint32_t character)
{
   struct held_key *entry;
   int i;

   if (held_count[port] == KEYPAD_MAX)
      return;

   for (i = 0; i < held_count[port]; i++)
      if (held[port][i].from_pad == from_pad && held[port][i].code == code)
         return;

   entry = &held[port][held_count[port]++];
   entry->from_pad = from_pad;
   entry->code = code;
   entry->character = character;
   keypad_unseen[port] = true;
   keypad_sync(port);
}

static void keypad_release(int port, bool from_pad, int code)
{
   int i;

   for (i = 0; i < held_count[port]; i++)
      if (held[port][i].from_pad == from_pad && held[port][i].code == code)
         break;

   if (i == held_count[port])
      return;

   held_count[port]--;
   memmove(&held[port][i], &held[port][i + 1], (size_t)(held_count[port] - i) * sizeof(held[0][0]));

   /* Deferred until input_after_run() so the core still sees a press that ended this frame. */
   if (!keypad_unseen[port])
      keypad_sync(port);
}

/* Drops a port's gamepad keypad presses, for when its gamepad is removed. */
static void keypad_drop_pad_entries(int port)
{
   int i;
   int kept = 0;

   for (i = 0; i < held_count[port]; i++)
      if (!held[port][i].from_pad)
         held[port][kept++] = held[port][i];

   if (kept == held_count[port])
      return;

   held_count[port] = kept;
   if (!keypad_unseen[port])
      keypad_sync(port);
}

/* Returns the player whose gamepad this is, or -1. */
static int port_for_pad(SDL_JoystickID id)
{
   int port;

   for (port = 0; port < PLAYER_COUNT; port++)
      if (pads[port] && SDL_GetGamepadID(pads[port]) == id)
         return port;

   return -1;
}

static bool pad_held(SDL_Gamepad *pad, int input)
{
   if (!pad || input < 0 || input > PAD_RIGHT_TRIGGER)
      return false;

   if (input == PAD_LEFT_TRIGGER)
      return SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > PAD_TRIGGER_THRESHOLD;
   if (input == PAD_RIGHT_TRIGGER)
      return SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > PAD_TRIGGER_THRESHOLD;

   return SDL_GetGamepadButton(pad, (SDL_GamepadButton)input);
}

static void latch_control(int port, int control)
{
   if (control < CONTROL_JOYPAD_COUNT)
      latched[port] |= (uint16_t)(1u << joypad_ids[control]);
}

/* Applies a press of one control. Joypad controls latch; keypad controls push a keypad entry. */
static void control_press(int port, bool from_pad, int code, int control)
{
   if (control < CONTROL_JOYPAD_COUNT)
      latch_control(port, control);
   else
      keypad_press(port, from_pad, code, keypad_character(control));
}

static void key_pressed(SDL_Scancode sc)
{
   int port;
   int control;

   if (sc == SDL_SCANCODE_UNKNOWN || blocked)
      return;

   for (port = 0; port < PLAYER_COUNT; port++)
      for (control = 0; control < CONTROL_COUNT; control++)
         if (maps[port].keys[control] == sc)
            control_press(port, false, sc, control);
}

static void pad_input_event(int port, int input, bool down)
{
   int control;

   if (!down)
   {
      keypad_release(port, true, input);
      return;
   }

   if (blocked)
      return;

   for (control = 0; control < CONTROL_COUNT; control++)
      if (maps[port].pads[control] == input)
         control_press(port, true, input, control);
}

/* Follows a trigger across the threshold. Updates even while blocked; pad_input_event() gates presses. */
static void trigger_edge(int port, int t, bool down)
{
   if (trigger_down[port][t] == down)
      return;

   trigger_down[port][t] = down;
   pad_input_event(port, PAD_LEFT_TRIGGER + t, down);
}

/* Re-reads a player's trigger state from the hardware. A trigger that is still held after a
 * reset does not press again until it is released and pressed. */
static void trigger_sync(int port)
{
   static const SDL_GamepadAxis axes[2] = { SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER };
   int t;

   for (t = 0; t < 2; t++)
   {
      bool down = pads[port] && SDL_GetGamepadAxis(pads[port], axes[t]) > PAD_TRIGGER_THRESHOLD;

      if (trigger_down[port][t] && !down)
         pad_input_event(port, PAD_LEFT_TRIGGER + t, false);
      trigger_down[port][t] = down;
   }
}

static void gamepad_added(SDL_JoystickID id)
{
   int port;

   if (port_for_pad(id) >= 0)
      return;

   for (port = 0; port < PLAYER_COUNT; port++)
   {
      if (!pads[port])
      {
         pads[port] = SDL_OpenGamepad(id);
         trigger_sync(port);
         return;
      }
   }
}

static void gamepad_removed(SDL_JoystickID id)
{
   int port = port_for_pad(id);

   if (port < 0)
      return;

   keypad_drop_pad_entries(port);
   SDL_CloseGamepad(pads[port]);
   pads[port] = NULL;
   memset(trigger_down[port], 0, sizeof(trigger_down[port]));
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

void input_set_bindings(const struct control_map bindings[PLAYER_COUNT])
{
   memcpy(maps, bindings, sizeof(maps));
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

   for (port = 0; port < PLAYER_COUNT; port++)
   {
      if (pads[port])
      {
         SDL_CloseGamepad(pads[port]);
         pads[port] = NULL;
      }
   }

   memset(held_count, 0, sizeof(held_count));
   memset(trigger_down, 0, sizeof(trigger_down));
}

void input_release_all(void)
{
   int port;

   for (port = 0; port < PLAYER_COUNT; port++)
   {
      held_count[port] = 0;
      keypad_unseen[port] = false;
      keypad_sync(port);
      latched[port] = 0;
      trigger_sync(port);
   }
}

void input_after_run(void)
{
   int port;

   for (port = 0; port < PLAYER_COUNT; port++)
   {
      keypad_unseen[port] = false;
      keypad_sync(port);
   }
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
   int port;

   switch (event->type)
   {
      case SDL_EVENT_KEY_DOWN:
         if (!event->key.repeat)
            key_pressed(event->key.scancode);
         break;

      case SDL_EVENT_KEY_UP:
         for (port = 0; port < PLAYER_COUNT; port++)
            keypad_release(port, false, event->key.scancode);
         break;

      case SDL_EVENT_GAMEPAD_ADDED:
         gamepad_added(event->gdevice.which);
         break;

      case SDL_EVENT_GAMEPAD_REMOVED:
         gamepad_removed(event->gdevice.which);
         break;

      case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
         port = port_for_pad(event->gbutton.which);
         if (port >= 0)
            pad_input_event(port, event->gbutton.button, true);
         break;

      case SDL_EVENT_GAMEPAD_BUTTON_UP:
         port = port_for_pad(event->gbutton.which);
         if (port >= 0)
            pad_input_event(port, event->gbutton.button, false);
         break;

      case SDL_EVENT_GAMEPAD_AXIS_MOTION:
         port = port_for_pad(event->gaxis.which);
         if (port >= 0)
         {
            bool down = event->gaxis.value > PAD_TRIGGER_THRESHOLD;

            if (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
               trigger_edge(port, 0, down);
            else if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
               trigger_edge(port, 1, down);
         }
         break;

      case SDL_EVENT_WINDOW_FOCUS_LOST:
         input_release_all();
         break;

      default:
         break;
   }
}

/* Returns the gamepad input in this event that capture should bind, or PAD_UNBOUND.
 * Stateless, so it does not track trigger edges. */
int input_pad_press_from_event(const SDL_Event *event)
{
   if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
      return pad_name(event->gbutton.button) ? event->gbutton.button : PAD_UNBOUND;

   if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event->gaxis.value > PAD_TRIGGER_THRESHOLD)
   {
      if (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
         return PAD_LEFT_TRIGGER;
      if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
         return PAD_RIGHT_TRIGGER;
   }

   return PAD_UNBOUND;
}

void frontend_input_poll(void)
{
   const bool *keys = SDL_GetKeyboardState(NULL);
   int port;
   int control;

   if (blocked)
   {
      memset(&snapshot, 0, sizeof(snapshot));
      memset(latched, 0, sizeof(latched));
      return;
   }

   for (port = 0; port < PLAYER_COUNT; port++)
   {
      SDL_Gamepad *pad = pads[port];
      uint16_t bits = 0;

      for (control = 0; control < CONTROL_JOYPAD_COUNT; control++)
      {
         SDL_Scancode sc = maps[port].keys[control];

         if ((sc != SDL_SCANCODE_UNKNOWN && keys[sc]) || pad_held(pad, maps[port].pads[control]))
            bits |= (uint16_t)(1u << joypad_ids[control]);
      }

      bits |= latched[port];
      latched[port] = 0;
      snapshot.buttons[port] = bits;

      snapshot.stick[port][0] = pad ? SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) : 0;
      snapshot.stick[port][1] = pad ? SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) : 0;
   }
}

int16_t frontend_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (port >= PLAYER_COUNT)
      return 0;

   if (device == RETRO_DEVICE_JOYPAD)
      return (id < 16 && ((snapshot.buttons[port] >> id) & 1u)) ? 1 : 0;

   if (device == RETRO_DEVICE_ANALOG && index == RETRO_DEVICE_INDEX_ANALOG_LEFT && id < 2)
      return snapshot.stick[port][id];

   return 0;
}
