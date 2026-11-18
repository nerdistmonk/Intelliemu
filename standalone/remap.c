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
static struct control_map *map;
static enum remap_device device;
static char title[16];
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

static const char *pad_text(int input)
{
   const char *name;

   if (input == PAD_UNBOUND)
      return "(none)";

   name = pad_name(input);
   return name ? name : "?";
}

static const char *binding_text(enum control c)
{
   if (device == REMAP_GAMEPAD)
      return pad_text(map->pads[c]);
   return key_text(map->keys[c]);
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

static bool report_bind(int displaced, const char *value)
{
   if (displaced >= 0)
      SDL_snprintf(status, sizeof(status), "Swapped with %s", control_label((enum control)displaced));
   else
      SDL_snprintf(status, sizeof(status), "Bound to %s", value);

   return true;
}

static bool bind_key(enum control c, SDL_Scancode sc)
{
   return report_bind(controls_bind_key(map, c, sc), key_text(sc));
}

static bool bind_pad(enum control c, int input)
{
   return report_bind(controls_bind_pad(map, c, input), pad_text(input));
}

static bool is_bound(enum control c)
{
   if (device == REMAP_GAMEPAD)
      return map->pads[c] != PAD_UNBOUND;
   return map->keys[c] != SDL_SCANCODE_UNKNOWN;
}

static bool unbind_key(enum control c)
{
   if (!is_bound(c))
   {
      SDL_strlcpy(status, "Already unbound", sizeof(status));
      return false;
   }

   if (device == REMAP_GAMEPAD)
      controls_bind_pad(map, c, PAD_UNBOUND);
   else
      controls_bind_key(map, c, SDL_SCANCODE_UNKNOWN);

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

void remap_open(struct control_map maps[PLAYER_COUNT], int player, enum remap_device d)
{
   map = &maps[player];
   device = d;
   SDL_snprintf(title, sizeof(title), "P%d %s", player + 1, d == REMAP_GAMEPAD ? "gamepad" : "keyboard");
   is_open = true;
   capturing = false;
   column = 0;
   row = 0;
   status[0] = '\0';
}

bool remap_wants_pad(void)
{
   return is_open && device == REMAP_GAMEPAD && capturing;
}

bool remap_handle_key(const SDL_KeyboardEvent *key)
{
   if (!is_open || key->repeat)
      return false;

   if (capturing && device == REMAP_GAMEPAD)
   {
      if (key->scancode == SDL_SCANCODE_ESCAPE)
      {
         capturing = false;
         SDL_strlcpy(status, "Cancelled", sizeof(status));
      }
      return false;
   }

   if (capturing)
      return capture_key(selected(), key);

   return navigate_key(key);
}

bool remap_handle_pad(int input)
{
   if (!remap_wants_pad() || input == PAD_UNBOUND)
      return false;

   capturing = false;
   return bind_pad(selected(), input);
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
            fit_text(text, sizeof(text), binding_text(c), value_chars);

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
      SDL_snprintf(text, sizeof(text), "%s | press %s, Esc cancels", title,
            device == REMAP_GAMEPAD ? "a pad button or trigger" : "a key");
   else
      SDL_snprintf(text, sizeof(text), "%s | %s: %s", title, control_label(sel), binding_text(sel));
   SDL_RenderDebugText(renderer, PAD, footer, text);

   SDL_SetRenderDrawColor(renderer, 255, 255, 160, 255);
   SDL_RenderDebugText(renderer, PAD, footer + LINE_HEIGHT, status);

   SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
   SDL_RenderDebugText(renderer, PAD, footer + 2 * LINE_HEIGHT, "Enter rebind  Del clear  Esc close");
}
