#ifndef INTELLIEMU_CONFIG_H
#define INTELLIEMU_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "controls.h"

#define DEFAULT_SCALE 3
#define MAX_SCALE 16
#define MAX_STATE_SLOT 9
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_VOLUME 100
#define SAMPLE_RATE_COUNT 4

/* Output rates offered in the Audio menu and accepted in config.ini. */
extern const int sample_rates[SAMPLE_RATE_COUNT];

/* Settings stored in ~/.config/Intelliemu/config.ini ($XDG_CONFIG_HOME is honored). */
struct settings
{
   long scale;
   char *system_dir;
   bool fullscreen;
   bool muted;
   bool aspect_4_3;
   bool xbrz;
   bool ecs_keyboard;
   int sample_rate;
   int volume;
   int state_slot;
   struct control_map bindings[PLAYER_COUNT];
   /* Gamepad saved for each port, restored when it reconnects. An empty string means none. */
   char gamepad_id[PLAYER_COUNT][GAMEPAD_ID_MAX];
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
