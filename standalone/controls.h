#ifndef INTELLIEMU_CONTROLS_H
#define INTELLIEMU_CONTROLS_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#define PLAYER_COUNT 2

/* Intellivision controls that a keyboard key or gamepad input can be bound to. Joypad controls come
 * first, then keypad buttons. */
enum control
{
   CONTROL_DISC_UP,
   CONTROL_DISC_DOWN,
   CONTROL_DISC_LEFT,
   CONTROL_DISC_RIGHT,
   CONTROL_LEFT_ACTION,
   CONTROL_RIGHT_ACTION,
   CONTROL_TOP_ACTION,
   CONTROL_BOTTOM_ACTION,
   CONTROL_START,
   CONTROL_SELECT,
   CONTROL_L_BUTTON,
   CONTROL_R_BUTTON,
   CONTROL_KEYPAD_1,
   CONTROL_KEYPAD_2,
   CONTROL_KEYPAD_3,
   CONTROL_KEYPAD_4,
   CONTROL_KEYPAD_5,
   CONTROL_KEYPAD_6,
   CONTROL_KEYPAD_7,
   CONTROL_KEYPAD_8,
   CONTROL_KEYPAD_9,
   CONTROL_KEYPAD_0,
   CONTROL_KEYPAD_CLEAR,
   CONTROL_KEYPAD_ENTER,
   CONTROL_COUNT
};

#define CONTROL_JOYPAD_COUNT 12

/* Gamepad inputs are SDL gamepad buttons, followed by the two analog triggers. */
#define PAD_LEFT_TRIGGER SDL_GAMEPAD_BUTTON_COUNT
#define PAD_RIGHT_TRIGGER (SDL_GAMEPAD_BUTTON_COUNT + 1)
#define PAD_UNBOUND (-1)
/* Trigger axis value (0 to 32767) at which a trigger counts as pressed. */
#define PAD_TRIGGER_THRESHOLD 8000

/* One player's bindings. A control can have a keyboard key and a gamepad input at the same time. */
struct control_map
{
   SDL_Scancode keys[CONTROL_COUNT];
   int pads[CONTROL_COUNT];
};

/* Name used in config.ini after p1_key_ or p1_pad_. */
const char *control_name(enum control c);
/* Short label for the remap screen. */
const char *control_label(enum control c);
/* Returns the control with this config name, or -1. */
int control_find(const char *name);
/* Keyboard default for a player (0 or 1). */
SDL_Scancode control_default_key(int player, enum control c);

/* Config name of a gamepad input, such as "a" or "lefttrigger". Returns NULL for an unnamed input. */
const char *pad_name(int input);
/* Returns the gamepad input with this config name, or PAD_UNBOUND. */
int pad_find(const char *name);

/* Restores keyboard defaults for both players and clears every gamepad binding. */
void controls_reset(struct control_map maps[PLAYER_COUNT]);

/* Binds key to c. If another control in the map had key, it takes c's previous key. Returns that
 * control, or -1. Binding SDL_SCANCODE_UNKNOWN only unbinds c. */
int controls_bind_key(struct control_map *map, enum control c, SDL_Scancode key);
/* Same as controls_bind_key, for gamepad inputs. PAD_UNBOUND only unbinds c. */
int controls_bind_pad(struct control_map *map, enum control c, int input);

/* Unbinds later controls that share a key or gamepad input with an earlier one. */
void controls_dedupe(struct control_map *map);

/* Keys the emulator uses itself: Escape, F1, F5, F9-F11, and Ctrl+O/W/Q. Must match the hotkeys in main.c. */
bool controls_is_reserved(SDL_Scancode sc, SDL_Keymod mod);

#endif
