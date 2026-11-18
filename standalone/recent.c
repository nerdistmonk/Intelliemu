#include <string.h>

#include <SDL3/SDL.h>

#include "config.h"
#include "recent.h"

#define RECENT_FILE "recent.txt"
#define RECENT_FILE_MAX (64 * 1024)

const char *path_basename(const char *path)
{
   const char *slash = strrchr(path, '/');
   const char *bslash = strrchr(path, '\\');

   if (bslash && (!slash || bslash > slash))
      slash = bslash;

   return slash ? slash + 1 : path;
}

static void recent_save(const struct recent_list *list)
{
   size_t length = 0;
   char *text;
   char *p;
   int i;

   for (i = 0; i < list->count; i++)
      length += strlen(list->paths[i]) + 1;

   text = SDL_malloc(length + 1);
   if (!text)
      return;

   p = text;
   for (i = 0; i < list->count; i++)
   {
      size_t n = strlen(list->paths[i]);

      memcpy(p, list->paths[i], n);
      p[n] = '\n';
      p += n + 1;
   }
   *p = '\0';

   config_write(RECENT_FILE, text);
   SDL_free(text);
}

void recent_load(struct recent_list *list)
{
   char *text = config_read(RECENT_FILE, RECENT_FILE_MAX);
   char *line;

   recent_free(list);

   for (line = text; line && *line && list->count < RECENT_MAX;)
   {
      char *end = strchr(line, '\n');

      if (end)
         *end = '\0';
      if (*line)
         list->paths[list->count++] = SDL_strdup(line);

      line = end ? end + 1 : NULL;
   }

   SDL_free(text);
}

void recent_add(struct recent_list *list, const char *path)
{
   char *copy;
   int i;

   if (!path || strchr(path, '\n'))
      return;

   copy = SDL_strdup(path);
   if (!copy)
      return;

   for (i = 0; i < list->count; i++)
      if (!strcmp(list->paths[i], path))
         break;

   /* A full list drops its oldest entry; a known path is moved to the front. */
   if (i == list->count && list->count == RECENT_MAX)
      i = RECENT_MAX - 1;

   if (i < list->count)
      SDL_free(list->paths[i]);
   else
      list->count++;

   memmove(&list->paths[1], &list->paths[0], (size_t)i * sizeof(list->paths[0]));
   list->paths[0] = copy;

   recent_save(list);
}

void recent_free(struct recent_list *list)
{
   int i;

   for (i = 0; i < list->count; i++)
      SDL_free(list->paths[i]);

   list->count = 0;
}
