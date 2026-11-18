#ifndef INTELLIEMU_RECENT_H
#define INTELLIEMU_RECENT_H

#define RECENT_MAX 8

struct recent_list
{
   char *paths[RECENT_MAX];
   int count;
};

/* Returns the file name part of a path, accepting '/' or '\\' separators. */
const char *path_basename(const char *path);

/* Loads the list kept in the user's preferences directory. */
void recent_load(struct recent_list *list);

/* Moves path to the front of the list and saves the list. */
void recent_add(struct recent_list *list, const char *path);

void recent_free(struct recent_list *list);

#endif
