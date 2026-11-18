#ifndef INTELLIEMU_ENV_H
#define INTELLIEMU_ENV_H

#include "libretro.h"

/*
 * Private environment call: the standalone frontend supplies the BIOS files it resolved.
 * Other frontends return false, and the core loads exec.bin and grom.bin from the system directory.
 */
#define INTELLIEMU_ENV_BIOS_PATHS (RETRO_ENVIRONMENT_PRIVATE | 1)

struct intelliemu_bios_paths
{
   const char *exec_path;
   const char *grom_path;
   const char *ecs_path;
};

#endif
