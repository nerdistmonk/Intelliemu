#ifndef INTELLIEMU_ENV_H
#define INTELLIEMU_ENV_H

#include <stdbool.h>
#include <stdint.h>

#include "libretro.h"

/*
 * Private environment call: the standalone frontend supplies the BIOS files it resolved.
 * Other frontends return false, and the core loads exec.bin and grom.bin from the system directory.
 */
#define INTELLIEMU_ENV_BIOS_PATHS (RETRO_ENVIRONMENT_PRIVATE | 1)

/*
 * Set to "enabled" by the standalone's Open BankSwitch ROM command, or by the
 * freeintv_bankswitch core option in other frontends. The core then loads the cart as a
 * bank-switched ROM (.cfg/.bin/.int). The core resets the value to "disabled" after each load.
 */
#define INTELLIEMU_VAR_BANKSWITCH "freeintv_bankswitch"

struct intelliemu_bios_paths
{
   const char *exec_path;
   const char *grom_path;
   const char *ecs_path;
};

/*
 * Keypad digits from the standalone frontend, called outside retro_run. port is the player (0 or 1).
 * character is the key's text: '1'-'9', '0', '[' for Clear, ']' for Enter. A release with character 0
 * or with the held character clears that player's keypad digit.
 */
void intelliemu_keypad_event(unsigned port, bool down, uint32_t character);

#endif
