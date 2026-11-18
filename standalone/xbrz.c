#include <stdint.h>

#include "xbrz.h"

/* 50/50 blend of two XRGB pixels, channel by channel. */
static uint32_t blend_half(uint32_t a, uint32_t b)
{
   return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b);
}

static const uint32_t *row_at(const void *base, size_t pitch, int y)
{
   return (const uint32_t *)((const unsigned char *)base + (size_t)y * pitch);
}

void xbrz_scale(const void *src, size_t src_pitch, uint32_t *dst, int width, int height)
{
   size_t out_pitch = (size_t)width * XBRZ_SCALE;

   for (int y = 0; y < height; y++)
   {
      const uint32_t *top = row_at(src, src_pitch, y > 0 ? y - 1 : 0);
      const uint32_t *mid = row_at(src, src_pitch, y);
      const uint32_t *bot = row_at(src, src_pitch, y + 1 < height ? y + 1 : height - 1);
      uint32_t *out0 = dst + (size_t)(XBRZ_SCALE * y) * out_pitch;
      uint32_t *out1 = out0 + out_pitch;

      for (int x = 0; x < width; x++)
      {
         int left = x > 0 ? x - 1 : 0;
         int right = x + 1 < width ? x + 1 : width - 1;
         uint32_t b = top[x];
         uint32_t d = mid[left];
         uint32_t e = mid[x];
         uint32_t f = mid[right];
         uint32_t h = bot[x];
         uint32_t e0 = e;
         uint32_t e1 = e;
         uint32_t e2 = e;
         uint32_t e3 = e;

         if (d == b && d != h && b != f)
            e0 = blend_half(e, d);
         if (b == f && b != d && f != h)
            e1 = blend_half(e, f);
         if (h == d && h != f && d != b)
            e2 = blend_half(e, d);
         if (f == h && f != b && h != d)
            e3 = blend_half(e, f);

         out0[2 * x] = e0;
         out0[2 * x + 1] = e1;
         out1[2 * x] = e2;
         out1[2 * x + 1] = e3;
      }
   }
}
