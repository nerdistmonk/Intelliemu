#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "config.h"

#define CONFIG_DIR_NAME "Intelliemu"
#define SETTINGS_FILE "config.ini"
#define SETTINGS_MAX_SIZE (64 * 1024)

const int sample_rates[SAMPLE_RATE_COUNT] = { 11025, 22050, 44100, 48000 };

char *config_dir(void)
{
   const char *xdg = getenv("XDG_CONFIG_HOME");
   const char *home = getenv("HOME");
   char *dir = NULL;

   if (xdg && xdg[0] == '/')
      SDL_asprintf(&dir, "%s/%s/", xdg, CONFIG_DIR_NAME);
   else if (home && home[0] == '/')
      SDL_asprintf(&dir, "%s/.config/%s/", home, CONFIG_DIR_NAME);

   return dir;
}

static char *config_path(const char *name)
{
   char *dir = config_dir();
   char *path = NULL;

   if (dir)
      SDL_asprintf(&path, "%s%s", dir, name);

   SDL_free(dir);
   return path;
}

static bool config_exists(const char *name)
{
   char *path = config_path(name);
   SDL_PathInfo info;
   bool exists = path && SDL_GetPathInfo(path, &info);

   SDL_free(path);
   return exists;
}

char *config_read(const char *name, size_t max_size)
{
   char *path = config_path(name);
   SDL_IOStream *io = path ? SDL_IOFromFile(path, "rb") : NULL;
   char *text = NULL;
   Sint64 size;

   SDL_free(path);
   if (!io)
      return NULL;

   size = SDL_GetIOSize(io);
   if (size > 0 && (size_t)size <= max_size)
   {
      text = SDL_malloc((size_t)size + 1);
      if (text && SDL_ReadIO(io, text, (size_t)size) == (size_t)size)
         text[size] = '\0';
      else
      {
         SDL_free(text);
         text = NULL;
      }
   }

   SDL_CloseIO(io);
   return text;
}

bool config_write(const char *name, const char *text)
{
   char *dir = config_dir();
   char *path = NULL;
   char *temp = NULL;
   SDL_IOStream *io = NULL;
   size_t length = strlen(text);
   bool ok = false;

   if (!dir)
   {
      fprintf(stderr, "cannot find config directory: HOME is not set\n");
      goto done;
   }

   if (!SDL_CreateDirectory(dir))
   {
      fprintf(stderr, "cannot create %s: %s\n", dir, SDL_GetError());
      goto done;
   }

   if (SDL_asprintf(&path, "%s%s", dir, name) < 0 || SDL_asprintf(&temp, "%s.tmp", path) < 0)
      goto done;

   io = SDL_IOFromFile(temp, "wb");
   if (!io)
   {
      fprintf(stderr, "cannot write %s: %s\n", temp, SDL_GetError());
      goto done;
   }

   ok = SDL_WriteIO(io, text, length) == length;
   ok = SDL_CloseIO(io) && ok;
   if (ok)
      ok = SDL_RenamePath(temp, path);

   if (!ok)
   {
      fprintf(stderr, "cannot save %s: %s\n", path, SDL_GetError());
      SDL_RemovePath(temp);
   }

done:
   SDL_free(temp);
   SDL_free(path);
   SDL_free(dir);
   return ok;
}

static void settings_defaults(struct settings *s)
{
   s->scale = DEFAULT_SCALE;
   s->system_dir = SDL_strdup(".");
   s->fullscreen = false;
   s->muted = false;
   s->aspect_4_3 = true;
   s->xbrz = false;
   s->ecs_keyboard = false;
   s->sample_rate = DEFAULT_SAMPLE_RATE;
   s->volume = DEFAULT_VOLUME;
   s->state_slot = 1;
   controls_reset(s->bindings);
   for (int p = 0; p < PLAYER_COUNT; p++)
      s->gamepad_id[p][0] = '\0';
}

static char *trim(char *text)
{
   char *end;

   while (*text && isspace((unsigned char)*text))
      text++;

   end = text + strlen(text);
   while (end > text && isspace((unsigned char)end[-1]))
      end--;
   *end = '\0';

   return text;
}

static void apply_flag(bool *flag, const char *value)
{
   if (!strcmp(value, "0") || !strcmp(value, "1"))
      *flag = value[0] == '1';
}

/* An empty value unbinds the control. An unknown or reserved name keeps the control's current key. */
static void apply_key(struct control_map *map, int control, const char *value)
{
   SDL_Scancode sc;

   if (!*value)
   {
      map->keys[control] = SDL_SCANCODE_UNKNOWN;
      return;
   }

   sc = SDL_GetScancodeFromName(value);
   if (sc != SDL_SCANCODE_UNKNOWN && !controls_is_reserved(sc, SDL_KMOD_NONE))
      map->keys[control] = sc;
}

/* An empty value unbinds the control. An unknown name keeps the control's current gamepad input. */
static void apply_pad(struct control_map *map, int control, const char *value)
{
   int input;

   if (!*value)
   {
      map->pads[control] = PAD_UNBOUND;
      return;
   }

   input = pad_find(value);
   if (input != PAD_UNBOUND)
      map->pads[control] = input;
}

/* Config name prefix of each port, indexed like the bindings. */
static const char *const port_names[PLAYER_COUNT] = { "right", "left" };

/* Splits a key such as right_key_disc_up into its port and the rest of the name. Returns -1 if the
 * key does not start with a port name. */
static int split_port(const char *key, const char **name)
{
   for (int p = 0; p < PLAYER_COUNT; p++)
   {
      size_t length = strlen(port_names[p]);

      if (!strncmp(key, port_names[p], length) && key[length] == '_')
      {
         *name = key + length + 1;
         return p;
      }
   }

   return -1;
}

/* Handles <port>_key_<control> and <port>_pad_<control>. */
static void apply_binding(struct settings *s, int port, const char *name, const char *value)
{
   bool pad;
   int control;

   if (!strncmp(name, "key_", 4))
      pad = false;
   else if (!strncmp(name, "pad_", 4))
      pad = true;
   else
      return;

   control = control_find(name + 4);
   if (control < 0)
      return;

   if (pad)
      apply_pad(&s->bindings[port], control, value);
   else
      apply_key(&s->bindings[port], control, value);
}

/* Handles <port>_gamepad and the <port>_key_ and <port>_pad_ bindings. Other keys are ignored. */
static void apply_port_setting(struct settings *s, const char *key, const char *value)
{
   const char *name;
   int port = split_port(key, &name);

   if (port < 0)
      return;

   if (!strcmp(name, "gamepad"))
      SDL_strlcpy(s->gamepad_id[port], value, sizeof s->gamepad_id[port]);
   else
      apply_binding(s, port, name, value);
}

static void apply_setting(struct settings *s, const char *key, const char *value)
{
   char *end;
   long number;

   if (!strcmp(key, "scale"))
   {
      number = strtol(value, &end, 10);
      if (*end == '\0' && number >= 1 && number <= MAX_SCALE)
         s->scale = number;
   }
   else if (!strcmp(key, "sample_rate"))
   {
      number = strtol(value, &end, 10);
      for (int i = 0; i < SAMPLE_RATE_COUNT; i++)
         if (*end == '\0' && number == sample_rates[i])
            s->sample_rate = (int)number;
   }
   else if (!strcmp(key, "volume"))
   {
      number = strtol(value, &end, 10);
      if (*end == '\0' && number >= 0 && number <= 100)
         s->volume = (int)number;
   }
   else if (!strcmp(key, "state_slot"))
   {
      number = strtol(value, &end, 10);
      if (*end == '\0' && number >= 1 && number <= MAX_STATE_SLOT)
         s->state_slot = (int)number;
   }
   else if (!strcmp(key, "fullscreen"))
      apply_flag(&s->fullscreen, value);
   else if (!strcmp(key, "muted"))
      apply_flag(&s->muted, value);
   else if (!strcmp(key, "aspect_4_3"))
      apply_flag(&s->aspect_4_3, value);
   else if (!strcmp(key, "xbrz"))
      apply_flag(&s->xbrz, value);
   else if (!strcmp(key, "ecs_keyboard"))
      apply_flag(&s->ecs_keyboard, value);
   else if (!strcmp(key, "system_dir") && *value)
   {
      char *copy = SDL_strdup(value);

      if (copy)
      {
         SDL_free(s->system_dir);
         s->system_dir = copy;
      }
   }
   else
      apply_port_setting(s, key, value);
}

void settings_load(struct settings *s)
{
   char *text;
   char *line;

   settings_defaults(s);

   if (!config_exists(SETTINGS_FILE))
   {
      settings_save(s);
      return;
   }

   text = config_read(SETTINGS_FILE, SETTINGS_MAX_SIZE);
   for (line = text; line;)
   {
      char *end = strchr(line, '\n');
      char *equals;

      if (end)
         *end = '\0';

      equals = strchr(line, '=');
      if (equals)
      {
         *equals = '\0';
         apply_setting(s, trim(line), trim(equals + 1));
      }

      line = end ? end + 1 : NULL;
   }

   for (int p = 0; p < PLAYER_COUNT; p++)
      controls_dedupe(&s->bindings[p]);
   SDL_free(text);
}

#define BINDING_LINE_MAX 96
#define GAMEPAD_LINE_MAX (GAMEPAD_ID_MAX + 32)
/* Room for every port's gamepad line, and every control's keyboard and gamepad line. */
#define BINDINGS_TEXT_MAX (PLAYER_COUNT * (GAMEPAD_LINE_MAX + 2 * CONTROL_COUNT * BINDING_LINE_MAX))

static void format_bindings(char *out, size_t size, const struct settings *s)
{
   size_t used = 0;

   out[0] = '\0';
   for (int p = 0; p < PLAYER_COUNT && used < size; p++)
   {
      int n = SDL_snprintf(out + used, size - used, "%s_gamepad = %s\n", port_names[p], s->gamepad_id[p]);

      if (n < 0)
         return;
      used += (size_t)n;

      for (int c = 0; c < CONTROL_COUNT && used < size; c++)
      {
         const char *value = s->bindings[p].keys[c] == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(s->bindings[p].keys[c]);

         n = SDL_snprintf(out + used, size - used, "%s_key_%s = %s\n", port_names[p], control_name(c), value);

         if (n < 0)
            return;
         used += (size_t)n;
      }

      for (int c = 0; c < CONTROL_COUNT && used < size; c++)
      {
         const char *value = pad_name(s->bindings[p].pads[c]);

         n = SDL_snprintf(out + used, size - used, "%s_pad_%s = %s\n", port_names[p], control_name(c), value ? value : "");

         if (n < 0)
            return;
         used += (size_t)n;
      }
   }
}

bool settings_save(const struct settings *s)
{
   char bindings_text[BINDINGS_TEXT_MAX];
   char *text = NULL;
   bool ok = false;

   format_bindings(bindings_text, sizeof bindings_text, s);
   if (SDL_asprintf(&text,
         "# Intelliemu settings. Use key = value; unknown keys are ignored.\n"
         "# scale: initial window scale, 1-%d (-x overrides it for one run)\n"
         "scale = %ld\n"
         "# system_dir: extra folder searched for BIOS files (-s overrides it for one run)\n"
         "system_dir = %s\n"
         "# fullscreen, muted, aspect_4_3, xbrz and ecs_keyboard: 1 or 0; also changed from the menu and hotkeys\n"
         "# aspect_4_3: 1 shows the picture at 4:3, 0 stretches it to fill the window\n"
         "# xbrz: 1 applies the xBRZ 2x smoothing filter\n"
         "# ecs_keyboard: 1 makes the keyboard act as the ECS keyboard and turns off the Intellivision keyboard bindings\n"
         "fullscreen = %d\n"
         "muted = %d\n"
         "aspect_4_3 = %d\n"
         "xbrz = %d\n"
         "ecs_keyboard = %d\n"
         "# sample_rate: audio output rate, 11025, 22050, 44100 or 48000; also changed from the Audio menu\n"
         "sample_rate = %d\n"
         "# volume: audio volume, 0-100; also changed from the Audio menu\n"
         "volume = %d\n"
         "# state_slot: save state slot, 1-%d; also changed with F9 or the State menu\n"
         "state_slot = %d\n"
         "# right_gamepad and left_gamepad: gamepad saved for each port; empty means none\n"
         "# right_key_* and left_key_*: keyboard key for each control (SDL key names, e.g. Z or Keypad 5); empty means unbound\n"
         "# right_pad_* and left_pad_*: gamepad input for each control (SDL names, e.g. a or lefttrigger); empty means unbound\n"
         "%s",
         MAX_SCALE, s->scale, s->system_dir ? s->system_dir : ".",
         s->fullscreen ? 1 : 0, s->muted ? 1 : 0, s->aspect_4_3 ? 1 : 0, s->xbrz ? 1 : 0,
         s->ecs_keyboard ? 1 : 0,
         s->sample_rate, s->volume,
         MAX_STATE_SLOT, s->state_slot, bindings_text) < 0)
      return false;

   ok = config_write(SETTINGS_FILE, text);
   SDL_free(text);
   return ok;
}

void settings_free(struct settings *s)
{
   SDL_free(s->system_dir);
   s->system_dir = NULL;
}
