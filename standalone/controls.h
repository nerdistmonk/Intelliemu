#ifndef INTELLIEMU_CONTROLS_H
#define INTELLIEMU_CONTROLS_H

#include <stdbool.h>

#include <SDL3/SDL.h>

/* Intellivision controls that a keyboard key can be bound to. Joypad controls come first,
 * then keypad buttons. */
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

/* Name used in config.ini, without the key_ prefix. */
const char *control_name(enum control c);
/* Short label for the remap screen. */
const char *control_label(enum control c);
SDL_Scancode control_default(enum control c);
/* Returns the control with this config name, or -1. */
int control_find(const char *name);

void controls_reset(SDL_Scancode keys[CONTROL_COUNT]);

/* Binds sc to c. If another control had sc, it takes c's previous key. Returns that control, or -1. */
int controls_bind(SDL_Scancode keys[CONTROL_COUNT], enum control c, SDL_Scancode sc);

/* Unbinds later controls that share a key with an earlier one. */
void controls_dedupe(SDL_Scancode keys[CONTROL_COUNT]);

/* Keys the emulator uses itself: Escape, F1, F5, F9-F11, and Ctrl+O/W/Q. Must match the hotkeys in main.c. */
bool controls_is_reserved(SDL_Scancode sc, SDL_Keymod mod);

#endif
