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
   controls_reset(s->keys);
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
static void apply_key(struct settings *s, const char *name, const char *value)
{
   int c = control_find(name);
   SDL_Scancode sc;

   if (c < 0)
      return;

   if (!*value)
   {
      s->keys[c] = SDL_SCANCODE_UNKNOWN;
      return;
   }

   sc = SDL_GetScancodeFromName(value);
   if (sc != SDL_SCANCODE_UNKNOWN && !controls_is_reserved(sc, SDL_KMOD_NONE))
      s->keys[c] = sc;
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
   else if (!strncmp(key, "key_", 4))
      apply_key(s, key + 4, value);
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

   controls_dedupe(s->keys);
   SDL_free(text);
}

#define KEY_LINE_MAX 96

static void format_keys(char *out, size_t size, const SDL_Scancode keys[CONTROL_COUNT])
{
   size_t used = 0;

   for (int i = 0; i < CONTROL_COUNT && used < size; i++)
   {
      int n = SDL_snprintf(out + used, size - used, "key_%s = %s\n", control_name(i), SDL_GetScancodeName(keys[i]));

      if (n < 0)
         break;
      used += (size_t)n;
   }
}

bool settings_save(const struct settings *s)
{
   char keys_text[CONTROL_COUNT * KEY_LINE_MAX];
   char *text = NULL;
   bool ok = false;

   format_keys(keys_text, sizeof keys_text, s->keys);
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
         "# key_*: keyboard key for each control (SDL key names, e.g. Z or Keypad 5); empty means unbound\n"
         "%s",
         MAX_SCALE, s->scale, s->system_dir ? s->system_dir : ".",
         s->fullscreen ? 1 : 0, s->muted ? 1 : 0, MAX_STATE_SLOT, s->state_slot, keys_text) < 0)
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
