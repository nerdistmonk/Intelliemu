#include <string.h>

#include "config.h"
#include "menu.h"

#define TITLE_COUNT 5
#define ITEM_HEIGHT 10
#define SEPARATOR_HEIGHT 4
#define CHAR_WIDTH 8
#define TEXT_PAD 4
#define LABEL_MAX 48
#define NAME_MAX 36
#define MAX_ITEMS (RECENT_MAX + 8)

struct menu_item
{
   char label[LABEL_MAX];
   const char *hint;
   enum menu_command command;
   int arg;
   bool enabled;
   bool separator;
};

static const char *const titles[TITLE_COUNT] = { "File", "Emulation", "State", "Video", "Audio" };
static int open_title = -1;

bool menu_is_open(void)
{
   return open_title >= 0;
}

void menu_close(void)
{
   open_title = -1;
}

static int title_width(int title)
{
   return ((int)strlen(titles[title]) + 2) * CHAR_WIDTH;
}

static int title_x(int title)
{
   int x = 0;
   int i;

   for (i = 0; i < title; i++)
      x += title_width(i);

   return x;
}

static int title_at(float x, float y)
{
   int i;

   if (y < 0 || y >= MENU_BAR_HEIGHT)
      return -1;

   for (i = 0; i < TITLE_COUNT; i++)
      if (x >= title_x(i) && x < title_x(i) + title_width(i))
         return i;

   return -1;
}

static int item_height(const struct menu_item *item)
{
   return item->separator ? SEPARATOR_HEIGHT : ITEM_HEIGHT;
}

static void add_item(struct menu_item *items, int *count, const char *label, const char *hint,
      enum menu_command command, int arg, bool enabled)
{
   struct menu_item *item = &items[(*count)++];

   SDL_strlcpy(item->label, label, sizeof(item->label));
   item->hint = hint;
   item->command = command;
   item->arg = arg;
   item->enabled = enabled;
   item->separator = false;
}

static void add_separator(struct menu_item *items, int *count)
{
   struct menu_item *item = &items[(*count)++];

   item->label[0] = '\0';
   item->hint = NULL;
   item->command = MENU_NONE;
   item->arg = 0;
   item->enabled = false;
   item->separator = true;
}

static int build_items(int title, const struct menu_state *state, struct menu_item *items)
{
   char label[LABEL_MAX];
   char name[NAME_MAX + 1];
   int n = 0;
   int i;

   switch (title)
   {
      case 0:
         add_item(items, &n, "Open ROM...", "Ctrl+O", MENU_OPEN_ROM, 0, true);
         add_item(items, &n, "Close ROM", "Ctrl+W", MENU_CLOSE_ROM, 0, state->game_running);
         add_separator(items, &n);
         if (state->recent->count == 0)
            add_item(items, &n, "(no recent ROMs)", NULL, MENU_NONE, 0, false);
         for (i = 0; i < state->recent->count; i++)
         {
            SDL_strlcpy(name, path_basename(state->recent->paths[i]), sizeof(name));
            SDL_snprintf(label, sizeof(label), "%d %s", i + 1, name);
            add_item(items, &n, label, NULL, MENU_RECENT, i, true);
         }
         add_separator(items, &n);
         add_item(items, &n, "About IntelliEmu...", NULL, MENU_ABOUT, 0, true);
         add_separator(items, &n);
         add_item(items, &n, "Exit", "Ctrl+Q", MENU_EXIT, 0, true);
         break;

      case 1:
         add_item(items, &n, "Reset", "F10", MENU_RESET, 0, state->game_running);
         add_item(items, &n, state->paused ? "Resume" : "Pause", NULL, MENU_TOGGLE_PAUSE, 0,
               state->game_running);
         add_separator(items, &n);
         add_item(items, &n, state->ecs_keyboard ? "[x] ECS keyboard" : "[ ] ECS keyboard", NULL,
               MENU_TOGGLE_ECS_KEYBOARD, 0, true);
         add_separator(items, &n);
         add_item(items, &n, "Remap Right keyboard...", NULL, MENU_REMAP_RIGHT_KEYS, 0, !state->ecs_keyboard);
         add_item(items, &n, "Remap Left keyboard...", NULL, MENU_REMAP_LEFT_KEYS, 0, !state->ecs_keyboard);
         add_item(items, &n, "Remap Right gamepad...", NULL, MENU_REMAP_RIGHT_PAD, 0, true);
         add_item(items, &n, "Remap Left gamepad...", NULL, MENU_REMAP_LEFT_PAD, 0, true);
         add_item(items, &n, "Swap Right and Left gamepads", NULL, MENU_SWAP_GAMEPADS, 0, true);
         add_item(items, &n, "Restore default bindings", NULL, MENU_RESET_KEYS, 0, true);
         break;

      case 2:
         SDL_snprintf(label, sizeof(label), "Save slot %d", state->state_slot);
         add_item(items, &n, label, "F1", MENU_SAVE_STATE, 0, state->game_running);
         SDL_snprintf(label, sizeof(label), "Load slot %d", state->state_slot);
         add_item(items, &n, label, "F5", MENU_LOAD_STATE, 0, state->game_running);
         add_item(items, &n, "Next slot", "F9", MENU_NEXT_SLOT, 0, true);
         break;

      case 3:
         add_item(items, &n, state->fullscreen ? "[x] Fullscreen" : "[ ] Fullscreen", "F11",
               MENU_TOGGLE_FULLSCREEN, 0, true);
         add_item(items, &n, state->aspect_4_3 ? "[x] 4:3 aspect" : "[ ] 4:3 aspect", NULL,
               MENU_TOGGLE_ASPECT, 0, true);
         add_item(items, &n, state->xbrz ? "[x] xBRZ filter" : "[ ] xBRZ filter", NULL,
               MENU_TOGGLE_XBRZ, 0, true);
         add_separator(items, &n);
         add_item(items, &n, "Screenshot (PNG)", "F12", MENU_SCREENSHOT, 0, state->game_running);
         break;

      case 4:
         add_item(items, &n, state->muted ? "[x] Mute" : "[ ] Mute", NULL, MENU_TOGGLE_MUTE, 0, true);
         add_separator(items, &n);
         for (i = 0; i < SAMPLE_RATE_COUNT; i++)
         {
            SDL_snprintf(label, sizeof(label), "%s %d Hz", state->sample_rate == sample_rates[i] ? "[x]" : "[ ]",
                  sample_rates[i]);
            add_item(items, &n, label, NULL, MENU_SET_SAMPLE_RATE, sample_rates[i], true);
         }
         add_separator(items, &n);
         SDL_snprintf(label, sizeof(label), "%s Volume 100%%", state->volume == 100 ? "[x]" : "[ ]");
         add_item(items, &n, label, NULL, MENU_SET_VOLUME, 100, true);
         SDL_snprintf(label, sizeof(label), "%s Volume 50%%", state->volume == 50 ? "[x]" : "[ ]");
         add_item(items, &n, label, NULL, MENU_SET_VOLUME, 50, true);
         break;

      default:
         break;
   }

   return n;
}

static SDL_FRect dropdown_rect(int title, const struct menu_state *state, const struct menu_item *items,
      int count)
{
   SDL_FRect rect;
   int text_w = 0;
   int height = 2;
   int i;

   for (i = 0; i < count; i++)
   {
      int w = 0;

      if (!items[i].separator)
      {
         w = (int)strlen(items[i].label) * CHAR_WIDTH;
         if (items[i].hint)
            w += ((int)strlen(items[i].hint) + 3) * CHAR_WIDTH;
      }

      if (w > text_w)
         text_w = w;
      height += item_height(&items[i]);
   }

   rect.w = (float)(text_w + 2 * TEXT_PAD);
   if (rect.w > state->canvas_width)
      rect.w = (float)state->canvas_width;

   rect.x = (float)title_x(title);
   if (rect.x + rect.w > state->canvas_width)
      rect.x = (float)state->canvas_width - rect.w;
   if (rect.x < 0)
      rect.x = 0;

   rect.y = MENU_BAR_HEIGHT;
   rect.h = (float)height;
   return rect;
}

bool menu_click(const struct menu_state *state, float x, float y, struct menu_action *action)
{
   struct menu_item items[MAX_ITEMS];
   SDL_FRect rect;
   float item_y;
   int count;
   int i;

   action->command = MENU_NONE;
   action->arg = 0;

   if (y >= 0 && y < MENU_BAR_HEIGHT)
   {
      int title = title_at(x, y);

      open_title = (title == open_title) ? -1 : title;
      return true;
   }

   if (open_title < 0)
      return false;

   count = build_items(open_title, state, items);
   rect = dropdown_rect(open_title, state, items, count);
   if (x < rect.x || x >= rect.x + rect.w || y < rect.y || y >= rect.y + rect.h)
   {
      open_title = -1;
      return true;
   }

   item_y = rect.y + 1;
   for (i = 0; i < count; i++)
   {
      float h = (float)item_height(&items[i]);

      if (y >= item_y && y < item_y + h)
      {
         if (items[i].enabled && !items[i].separator)
         {
            action->command = items[i].command;
            action->arg = items[i].arg;
            open_title = -1;
         }
         return true;
      }
      item_y += h;
   }

   return true;
}

void menu_hover(float x, float y)
{
   int title;

   if (open_title < 0)
      return;

   title = title_at(x, y);
   if (title >= 0)
      open_title = title;
}

void menu_render(SDL_Renderer *renderer, const struct menu_state *state)
{
   SDL_FRect bar = { 0, 0, (float)state->canvas_width, MENU_BAR_HEIGHT };
   int i;

   SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
   SDL_RenderFillRect(renderer, &bar);

   for (i = 0; i < TITLE_COUNT; i++)
   {
      float x = (float)title_x(i);

      if (i == open_title)
      {
         SDL_FRect box = { x, 0, (float)title_width(i), MENU_BAR_HEIGHT };

         SDL_SetRenderDrawColor(renderer, 90, 90, 130, 255);
         SDL_RenderFillRect(renderer, &box);
      }

      SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
      SDL_RenderDebugText(renderer, x + CHAR_WIDTH, 2, titles[i]);
   }

   if (open_title >= 0)
   {
      struct menu_item items[MAX_ITEMS];
      int count = build_items(open_title, state, items);
      SDL_FRect rect = dropdown_rect(open_title, state, items, count);
      float y = rect.y + 1;

      SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
      SDL_RenderFillRect(renderer, &rect);
      SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
      SDL_RenderRect(renderer, &rect);

      for (i = 0; i < count; i++)
      {
         float h = (float)item_height(&items[i]);

         if (items[i].separator)
         {
            SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
            SDL_RenderLine(renderer, rect.x + 2, y + h / 2, rect.x + rect.w - 2, y + h / 2);
         }
         else
         {
            Uint8 shade = items[i].enabled ? 235 : 110;

            SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
            SDL_RenderDebugText(renderer, rect.x + TEXT_PAD, y + 1, items[i].label);
            if (items[i].hint)
               SDL_RenderDebugText(renderer, rect.x + rect.w - TEXT_PAD - (float)(strlen(items[i].hint) * CHAR_WIDTH),
                     y + 1, items[i].hint);
         }

         y += h;
      }
   }
}
