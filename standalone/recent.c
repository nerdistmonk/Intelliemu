#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

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

static char *list_path(void)
{
   char *dir = SDL_GetPrefPath(NULL, "Intelliemu");
   char *path = NULL;

   if (dir)
      SDL_asprintf(&path, "%s%s", dir, RECENT_FILE);

   SDL_free(dir);
   return path;
}

static void recent_save(const struct recent_list *list)
{
   char *path = list_path();
   SDL_IOStream *io;
   int i;

   if (!path)
      return;

   io = SDL_IOFromFile(path, "wb");
   if (!io)
   {
      fprintf(stderr, "cannot save recent ROM list: %s\n", SDL_GetError());
   }
   else
   {
      for (i = 0; i < list->count; i++)
      {
         SDL_WriteIO(io, list->paths[i], strlen(list->paths[i]));
         SDL_WriteIO(io, "\n", 1);
      }
      SDL_CloseIO(io);
   }

   SDL_free(path);
}

void recent_load(struct recent_list *list)
{
   char *path = list_path();
   SDL_IOStream *io = path ? SDL_IOFromFile(path, "rb") : NULL;
   char *text = NULL;
   char *line;

   recent_free(list);

   if (io)
   {
      Sint64 size = SDL_GetIOSize(io);

      if (size > 0 && size <= RECENT_FILE_MAX)
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
   }

   SDL_free(path);

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
