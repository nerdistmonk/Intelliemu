#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "config.h"

#define CONFIG_DIR_NAME "Intelliemu"
#define SETTINGS_FILE "config.ini"
#define SETTINGS_MAX_SIZE (64 * 1024)

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
   s->state_slot = 1;
   controls_reset(s->bindings);
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

/* Handles p1_key_<control>, p2_key_<control>, p1_pad_<control> and p2_pad_<control>. Other keys are ignored. */
static void apply_binding(struct settings *s, const char *key, const char *value)
{
   int player;
   int control;
   bool pad;

   if (key[0] != 'p' || key[1] < '1' || key[1] >= '1' + PLAYER_COUNT || key[2] != '_')
      return;

   player = key[1] - '1';
   if (!strncmp(key + 3, "key_", 4))
      pad = false;
   else if (!strncmp(key + 3, "pad_", 4))
      pad = true;
   else
      return;

   control = control_find(key + 7);
   if (control < 0)
      return;

   if (pad)
      apply_pad(&s->bindings[player], control, value);
   else
      apply_key(&s->bindings[player], control, value);
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
      apply_binding(s, key, value);
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
/* Room for every control's keyboard and gamepad line, for each player. */
#define BINDINGS_TEXT_MAX (PLAYER_COUNT * 2 * CONTROL_COUNT * BINDING_LINE_MAX)

static void format_bindings(char *out, size_t size, const struct control_map maps[PLAYER_COUNT])
{
   size_t used = 0;

   out[0] = '\0';
   for (int p = 0; p < PLAYER_COUNT && used < size; p++)
   {
      for (int c = 0; c < CONTROL_COUNT && used < size; c++)
      {
         const char *value = maps[p].keys[c] == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(maps[p].keys[c]);
         int n = SDL_snprintf(out + used, size - used, "p%d_key_%s = %s\n", p + 1, control_name(c), value);

         if (n < 0)
            return;
         used += (size_t)n;
      }

      for (int c = 0; c < CONTROL_COUNT && used < size; c++)
      {
         const char *value = pad_name(maps[p].pads[c]);
         int n = SDL_snprintf(out + used, size - used, "p%d_pad_%s = %s\n", p + 1, control_name(c), value ? value : "");

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

   format_bindings(bindings_text, sizeof bindings_text, s->bindings);
   if (SDL_asprintf(&text,
         "# Intelliemu settings. Use key = value; unknown keys are ignored.\n"
         "# scale: initial window scale, 1-%d (-x overrides it for one run)\n"
         "scale = %ld\n"
         "# system_dir: extra folder searched for BIOS files (-s overrides it for one run)\n"
         "system_dir = %s\n"
         "# fullscreen and muted: 1 or 0; also changed from the menu and hotkeys\n"
         "fullscreen = %d\n"
         "muted = %d\n"
         "# state_slot: save state slot, 1-%d; also changed with F9 or the State menu\n"
         "state_slot = %d\n"
         "# p1_key_* and p2_key_*: keyboard key for each control (SDL key names, e.g. Z or Keypad 5); empty means unbound\n"
         "# p1_pad_* and p2_pad_*: gamepad input for each control (SDL names, e.g. a or lefttrigger); empty means unbound\n"
         "%s",
         MAX_SCALE, s->scale, s->system_dir ? s->system_dir : ".",
         s->fullscreen ? 1 : 0, s->muted ? 1 : 0, MAX_STATE_SLOT, s->state_slot, bindings_text) < 0)
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
