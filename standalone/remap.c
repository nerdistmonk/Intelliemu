#include <string.h>

#include "menu.h"
#include "remap.h"

#define ROWS CONTROL_JOYPAD_COUNT
#define COLUMN_COUNT 2
#define CHAR_WIDTH 8
#define PAD 4
#define GAP 8
#define HEADER_Y 4
#define ROW_TOP 18
#define ROW_HEIGHT 13
#define FOOTER_GAP 6
#define LINE_HEIGHT 12
#define TEXT_MAX 96

static bool is_open;
static SDL_Scancode *keys;
static int column;
static int row;
static bool capturing;
static char status[TEXT_MAX];

bool remap_is_open(void)
{
   return is_open;
}

static enum control selected(void)
{
   return (enum control)(column * ROWS + row);
}

static const char *key_text(SDL_Scancode sc)
{
   const char *name;

   if (sc == SDL_SCANCODE_UNKNOWN)
      return "(none)";

   name = SDL_GetScancodeName(sc);
   return name[0] ? name : "?";
}

/* Shortens src to max_chars, ending in ".." when cut. */
static void fit_text(char *dst, size_t size, const char *src, int max_chars)
{
   int len = (int)strlen(src);

   if (len <= max_chars)
   {
      SDL_strlcpy(dst, src, size);
      return;
   }

   if (max_chars < 3 || (size_t)max_chars >= size)
   {
      dst[0] = '\0';
      return;
   }

   memcpy(dst, src, (size_t)max_chars - 2);
   memcpy(dst + max_chars - 2, "..", 2);
   dst[max_chars] = '\0';
}

static void close_remap(void)
{
   is_open = false;
   capturing = false;
}

static bool bind_key(enum control c, SDL_Scancode sc)
{
   int displaced = controls_bind(keys, c, sc);

   if (displaced >= 0)
      SDL_snprintf(status, sizeof(status), "Swapped with %s", control_label((enum control)displaced));
   else
      SDL_snprintf(status, sizeof(status), "Bound to %s", key_text(sc));

   return true;
}

static bool unbind_key(enum control c)
{
   if (keys[c] == SDL_SCANCODE_UNKNOWN)
   {
      SDL_strlcpy(status, "Already unbound", sizeof(status));
      return false;
   }

   keys[c] = SDL_SCANCODE_UNKNOWN;
   SDL_snprintf(status, sizeof(status), "Unbound %s", control_label(c));
   return true;
}

static bool capture_key(enum control c, const SDL_KeyboardEvent *key)
{
   if (key->scancode == SDL_SCANCODE_ESCAPE)
   {
      capturing = false;
      SDL_strlcpy(status, "Cancelled", sizeof(status));
      return false;
   }

   if (controls_is_reserved(key->scancode, key->mod))
   {
      SDL_strlcpy(status, "Reserved for the emulator", sizeof(status));
      return false;
   }

   if (!SDL_GetScancodeName(key->scancode)[0])
   {
      SDL_strlcpy(status, "Key not supported", sizeof(status));
      return false;
   }

   capturing = false;
   return bind_key(c, key->scancode);
}

static bool navigate_key(const SDL_KeyboardEvent *key)
{
   status[0] = '\0';

   switch (key->scancode)
   {
      case SDL_SCANCODE_ESCAPE:
         close_remap();
         break;
      case SDL_SCANCODE_UP:
         row = (row + ROWS - 1) % ROWS;
         break;
      case SDL_SCANCODE_DOWN:
         row = (row + 1) % ROWS;
         break;
      case SDL_SCANCODE_LEFT:
         column = 0;
         break;
      case SDL_SCANCODE_RIGHT:
         column = COLUMN_COUNT - 1;
         break;
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_KP_ENTER:
         capturing = true;
         break;
      case SDL_SCANCODE_DELETE:
         return unbind_key(selected());
      default:
         break;
   }

   return false;
}

void remap_open(SDL_Scancode map[CONTROL_COUNT])
{
   keys = map;
   is_open = true;
   capturing = false;
   column = 0;
   row = 0;
   status[0] = '\0';
}

bool remap_handle_key(const SDL_KeyboardEvent *key)
{
   if (!is_open || key->repeat)
      return false;

   if (capturing)
      return capture_key(selected(), key);

   return navigate_key(key);
}

void remap_render(SDL_Renderer *renderer, int width, int height)
{
   const int col_w = width / COLUMN_COUNT;
   const float top = (float)MENU_BAR_HEIGHT;
   const enum control sel = selected();
   SDL_FRect panel = { 0, top, (float)width, (float)(height - MENU_BAR_HEIGHT) };
   char text[TEXT_MAX];
   float footer;
   int col;
   int r;

   SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
   SDL_RenderFillRect(renderer, &panel);

   SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
   SDL_RenderDebugText(renderer, PAD, top + HEADER_Y, "Controller");
   SDL_RenderDebugText(renderer, (float)(col_w + PAD), top + HEADER_Y, "Keypad");

   for (col = 0; col < COLUMN_COUNT; col++)
   {
      for (r = 0; r < ROWS; r++)
      {
         enum control c = (enum control)(col * ROWS + r);
         float x = (float)(col * col_w);
         float y = top + ROW_TOP + (float)(r * ROW_HEIGHT);
         const char *label = control_label(c);
         int value_chars = (col_w - 2 * PAD - GAP - (int)strlen(label) * CHAR_WIDTH) / CHAR_WIDTH;

         if (c == sel)
         {
            SDL_FRect bar = { x + 1, y, (float)(col_w - 2), (float)(ROW_HEIGHT - 1) };

            SDL_SetRenderDrawColor(renderer, 60, 60, 100, 255);
            SDL_RenderFillRect(renderer, &bar);
         }

         if (capturing && c == sel)
            SDL_strlcpy(text, "...", sizeof(text));
         else
            fit_text(text, sizeof(text), key_text(keys[c]), value_chars);

         SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
         SDL_RenderDebugText(renderer, x + PAD, y + 3, label);

         SDL_SetRenderDrawColor(renderer, 255, 255, 160, 255);
         SDL_RenderDebugText(renderer, x + col_w - PAD - (float)(strlen(text) * CHAR_WIDTH), y + 3, text);
      }
   }

   SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
   SDL_RenderLine(renderer, (float)col_w, top + ROW_TOP, (float)col_w, top + ROW_TOP + ROWS * ROW_HEIGHT);

   footer = top + ROW_TOP + ROWS * ROW_HEIGHT + FOOTER_GAP;

   SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
   if (capturing)
      SDL_RenderDebugText(renderer, PAD, footer, "Press a key, Esc cancels");
   else
   {
      SDL_snprintf(text, sizeof(text), "%s: %s", control_label(sel), key_text(keys[sel]));
      SDL_RenderDebugText(renderer, PAD, footer, text);
   }

   SDL_SetRenderDrawColor(renderer, 255, 255, 160, 255);
   SDL_RenderDebugText(renderer, PAD, footer + LINE_HEIGHT, status);

   SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
   SDL_RenderDebugText(renderer, PAD, footer + 2 * LINE_HEIGHT, "Enter rebind  Del clear  Esc close");
}
