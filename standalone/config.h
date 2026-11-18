#ifndef INTELLIEMU_CONFIG_H
#define INTELLIEMU_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "controls.h"

#define DEFAULT_SCALE 3
#define MAX_SCALE 16
#define MAX_STATE_SLOT 9

/* Settings stored in ~/.config/Intelliemu/config.ini ($XDG_CONFIG_HOME is honored). */
struct settings
{
   long scale;
   char *system_dir;
   bool fullscreen;
   bool muted;
   bool p1_left;
   int state_slot;
   struct control_map bindings[PLAYER_COUNT];
};

/* Fills s with defaults, then with values from config.ini. Creates the file if it does not exist. */
void settings_load(struct settings *s);

/* Writes s to config.ini. Returns false and reports on stderr if the write fails. */
bool settings_save(const struct settings *s);

void settings_free(struct settings *s);

/* Returns ~/.config/Intelliemu/ (with trailing slash), or NULL. Caller frees. */
char *config_dir(void);

/* Reads a file from the config directory. Returns NULL if it is missing or larger than max_size. */
char *config_read(const char *name, size_t max_size);

/* Replaces a file in the config directory, creating the directory if needed. */
bool config_write(const char *name, const char *text);

#endif
