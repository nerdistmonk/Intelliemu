#ifndef INTELLIEMU_BIOS_H
#define INTELLIEMU_BIOS_H

#include <stddef.h>

#define BIOS_EXEC_SIZE 8192
#define BIOS_GROM_SIZE 2048

/* Full paths to the BIOS images, or NULL when not found. Release with bios_free. */
struct bios_files
{
   char *exec_path;
   char *grom_path;
};

/*
 * Searches ~/.config/Intelliemu/bios/, bios/ beside the executable, and system_dir.
 * Names are matched case-insensitively (exec.bin, INTV_EROM.bin, ...) and the size must match.
 * Prints a warning for each image that is not found.
 */
void bios_find(const char *system_dir, struct bios_files *files);
void bios_free(struct bios_files *files);

#endif
