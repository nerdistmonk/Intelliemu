#ifndef INTELLIEMU_XBRZ_H
#define INTELLIEMU_XBRZ_H

#include <stddef.h>
#include <stdint.h>

#define XBRZ_SCALE 2

/* Doubles a 32-bit XRGB frame in each direction, smoothing diagonal edges.
 * src rows are src_pitch bytes apart. dst is packed: width * 2 pixels per row. */
void xbrz_scale(const void *src, size_t src_pitch, uint32_t *dst, int width, int height);

#endif
