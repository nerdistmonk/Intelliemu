#ifndef ECS_KEYBOARD_DEVICE_H
#define ECS_KEYBOARD_DEVICE_H

#include "libretro.h"

/*
	Libretro input device the standalone frontend uses to send the ECS keyboard matrix to the core.
	Other frontends do not report anything on it, so the ECS keyboard stays off there.
*/
#define ECS_KEYBOARD_DEVICE RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0x45)

/* Id of matrix key row 0-6 in column 0-7; 1 while the key is held. */
#define ECS_KEYBOARD_KEY(col, row) ((col) * 8 + (row))

/* Id that reports 1 while the ECS keyboard is switched on. */
#define ECS_KEYBOARD_ENABLE_ID 63

#endif
