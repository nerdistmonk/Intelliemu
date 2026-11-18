#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "bios.h"
#include "config.h"

/* Matched against the normalized name: see name_key. */
static const char *const exec_names[] = { "exec", "executive", "execrom", "executiverom", "erom", NULL };
static const char *const grom_names[] = { "grom", "graphics", "graphicsrom", NULL };

/* Lowercases the name, drops non-alphanumerics, the .bin/.rom extension, and an "intv" prefix. */
static bool name_key(const char *filename, char *key, size_t key_size)
{
   const char *ext = strrchr(filename, '.');
   size_t n = 0;

   if (!ext || (SDL_strcasecmp(ext, ".bin") != 0 && SDL_strcasecmp(ext, ".rom") != 0))
      return false;

   for (const char *p = filename; p < ext && n + 1 < key_size; p++)
   {
      if (isalnum((unsigned char)*p))
         key[n++] = (char)tolower((unsigned char)*p);
   }
   key[n] = '\0';

   if (n > 4 && strncmp(key, "intv", 4) == 0)
      memmove(key, key + 4, n - 3);

   return true;
}

static bool name_listed(const char *key, const char *const *names)
{
   for (; *names; names++)
   {
      if (strcmp(key, *names) == 0)
         return true;
   }
   return false;
}

struct search
{
   const char *const *names;
   size_t size;
   char *path;
};

static SDL_EnumerationResult SDLCALL visit(void *userdata, const char *dirname, const char *fname)
{
   struct search *s = userdata;
   char key[32];
   char *path = NULL;
   const char *sep;
   size_t len;
   SDL_PathInfo info;

   if (!name_key(fname, key, sizeof(key)) || !name_listed(key, s->names))
      return SDL_ENUM_CONTINUE;

   len = strlen(dirname);
   sep = (len > 0 && dirname[len - 1] == '/') ? "" : "/";
   if (SDL_asprintf(&path, "%s%s%s", dirname, sep, fname) < 0)
      return SDL_ENUM_CONTINUE;

   if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE)
   {
      SDL_free(path);
      return SDL_ENUM_CONTINUE;
   }

   if ((size_t)info.size == s->size)
   {
      s->path = path;
      return SDL_ENUM_SUCCESS;
   }

   fprintf(stderr, "ignoring %s: %llu bytes, expected %zu\n", path, (unsigned long long)info.size, s->size);
   SDL_free(path);
   return SDL_ENUM_CONTINUE;
}

static char *find_image(char *const *dirs, size_t count, const char *const *names, size_t size)
{
   size_t i;

   for (i = 0; i < count; i++)
   {
      struct search s = { names, size, NULL };

      SDL_EnumerateDirectory(dirs[i], visit, &s);
      if (s.path)
         return s.path;
   }
   return NULL;
}

static void report_missing(const char *what, const char *example, size_t size, char *const *dirs, size_t count)
{
   size_t i;

   fprintf(stderr, "warning: %s (%zu bytes, e.g. %s) not found in:\n", what, size, example);
   for (i = 0; i < count; i++)
      fprintf(stderr, "  %s\n", dirs[i]);
}

void bios_find(const char *system_dir, struct bios_files *files)
{
   const char *base = SDL_GetBasePath();
   char *config = config_dir();
   char *dirs[3] = { NULL, NULL, NULL };
   size_t count = 0;
   char *dir = NULL;

   if (config && SDL_asprintf(&dir, "%sbios", config) >= 0)
      dirs[count++] = dir;
   if (base && SDL_asprintf(&dir, "%sbios", base) >= 0)
      dirs[count++] = dir;
   if (system_dir)
      dirs[count++] = SDL_strdup(system_dir);

   files->exec_path = find_image(dirs, count, exec_names, BIOS_EXEC_SIZE);
   files->grom_path = find_image(dirs, count, grom_names, BIOS_GROM_SIZE);

   if (!files->exec_path)
      report_missing("Executive ROM", "exec.bin or INTV_EROM.bin", BIOS_EXEC_SIZE, dirs, count);
   if (!files->grom_path)
      report_missing("Graphics ROM", "grom.bin or INTV_GROM.bin", BIOS_GROM_SIZE, dirs, count);

   for (size_t i = 0; i < count; i++)
      SDL_free(dirs[i]);
   SDL_free(config);
}

void bios_free(struct bios_files *files)
{
   SDL_free(files->exec_path);
   SDL_free(files->grom_path);
   files->exec_path = NULL;
   files->grom_path = NULL;
}
