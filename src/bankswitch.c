#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <streams/file_stream.h>
#include "bankswitch.h"
#include "ecs.h"
#include "memory.h"

#define BANK_MAX_IMAGE  (1024 * 1024)
#define BANK_MAX_SPANS  512
#define BANK_REGIONS    16
#define BANK_PATH_MAX   4096

typedef struct {
    unsigned start; /* first word index in the image file */
    unsigned len;   /* length in words */
    unsigned addr;  /* first Memory[] address */
    bool paged;
    unsigned page;
} bank_span_t;

bool bankswitch_active = false;

static uint8_t *image = NULL;
static size_t image_words = 0;
static bank_span_t spans[BANK_MAX_SPANS];
static int nspans = 0;
static bool rom_mapped[0x10000];
static bool region_touched[BANK_REGIONS];
static bool region_paged[BANK_REGIONS];

static bool has_ext_ci(const char *path, const char *ext)
{
    size_t pl = strlen(path), el = strlen(ext);
    if (pl <= el + 1 || path[pl - el - 1] != '.')
        return false;
    for (size_t i = 0; i < el; i++)
        if (tolower((unsigned char)path[pl - el + i]) != ext[i])
            return false;
    return true;
}

static bool replace_ext(const char *path, const char *ext, char *out, size_t cap)
{
    size_t base = strlen(path);
    while (base > 0 && path[base - 1] != '.')
        base--;
    if (base == 0)
        return false;
    int n = snprintf(out, cap, "%.*s%s", (int)base, path, ext);
    return n > 0 && (size_t)n < cap;
}

static RFILE *open_read(const char *path)
{
    return filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static unsigned image_word(unsigned i)
{
    return ((unsigned)image[2 * i] << 8) | image[2 * i + 1];
}

/* Fills every address in region r: unselected paged addresses read 0xFFFF,
 * fixed and currently selected spans are copied in. Addresses that no span
 * covers (RAM, GRAM, I/O) are never touched. */
static void apply_region(int r, unsigned page)
{
    unsigned base = (unsigned)r << 12;
    unsigned end = base + 0x1000;

    for (unsigned a = base; a < end; a++)
        if (rom_mapped[a])
            Memory[a] = 0xFFFF;

    for (int i = 0; i < nspans; i++) {
        const bank_span_t *s = &spans[i];
        if (s->paged && s->page != page)
            continue;

        unsigned lo = s->addr;
        unsigned hi = s->addr + s->len;
        if (hi <= base || lo >= end)
            continue;
        if (lo < base)
            lo = base;
        if (hi > end)
            hi = end;

        for (unsigned a = lo; a < hi; a++)
            Memory[a] = image_word(s->start + (a - s->addr));
    }
}

static void apply_all_regions(unsigned page)
{
    for (int r = 0; r < BANK_REGIONS; r++)
        if (region_touched[r])
            apply_region(r, page);
}

static bool add_span(unsigned s, unsigned e, unsigned a, bool paged, unsigned page)
{
    if (nspans >= BANK_MAX_SPANS || e < s || e >= image_words)
        return false;

    unsigned len = e - s + 1;
    if (a + len > 0x10000 || (paged && page > 0xF))
        return false;

    spans[nspans].start = s;
    spans[nspans].len = len;
    spans[nspans].addr = a;
    spans[nspans].paged = paged;
    spans[nspans].page = page;
    nspans++;

    for (unsigned i = 0; i < len; i++) {
        unsigned addr = a + i;
        rom_mapped[addr] = true;
        region_touched[addr >> 12] = true;
        if (paged)
            region_paged[addr >> 12] = true;
    }
    return true;
}

static bool parse_cfg(const char *cfg_path)
{
    RFILE *f = open_read(cfg_path);
    char line[512];
    bool in_mapping = false;
    bool ok = true;

    if (!f)
        return false;

    while (ok && filestream_gets(f, line, sizeof(line))) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '[') {
            in_mapping = strncmp(p, "[mapping]", 9) == 0;
            continue;
        }
        if (!in_mapping || *p != '$')
            continue;

        unsigned s, e, a, pg = 0;
        int n = sscanf(p, "$%x - $%x = $%x PAGE %x", &s, &e, &a, &pg);
        if (n == 4)
            ok = add_span(s, e, a, true, pg);
        else if (n == 3)
            ok = add_span(s, e, a, false, 0);
        else
            ok = false;
    }

    filestream_close(f);
    return ok;
}

static bool read_image(const char *img_path)
{
    RFILE *f = open_read(img_path);
    int64_t size, got;

    if (!f)
        return false;

    size = filestream_get_size(f);
    if (size < 2 || size > BANK_MAX_IMAGE) {
        filestream_close(f);
        return false;
    }

    image = malloc((size_t)size);
    if (!image) {
        filestream_close(f);
        return false;
    }

    got = filestream_read(f, image, size);
    filestream_close(f);
    if (got != size)
        return false;

    image_words = (size_t)size / 2;
    return true;
}

static bool image_exists(const char *path)
{
    RFILE *f = open_read(path);
    if (!f)
        return false;
    filestream_close(f);
    return true;
}

void bankswitch_unload(void)
{
    free(image);
    image = NULL;
    image_words = 0;
    nspans = 0;
    memset(rom_mapped, 0, sizeof(rom_mapped));
    memset(region_touched, 0, sizeof(region_touched));
    memset(region_paged, 0, sizeof(region_paged));
    bankswitch_active = false;
}

bool bankswitch_load(const char *path)
{
    char cfg_path[BANK_PATH_MAX];
    char img_path[BANK_PATH_MAX];
    bool ok = false;

    bankswitch_unload();

    if (!path)
        return false;

    if (has_ext_ci(path, "cfg")) {
        strncpy(cfg_path, path, sizeof(cfg_path) - 1);
        cfg_path[sizeof(cfg_path) - 1] = '\0';
        if (!(replace_ext(path, "int", img_path, sizeof(img_path)) && image_exists(img_path)) &&
            !(replace_ext(path, "bin", img_path, sizeof(img_path)) && image_exists(img_path)))
            return false;
    } else if (has_ext_ci(path, "bin") || has_ext_ci(path, "int")) {
        strncpy(img_path, path, sizeof(img_path) - 1);
        img_path[sizeof(img_path) - 1] = '\0';
        if (!replace_ext(path, "cfg", cfg_path, sizeof(cfg_path)))
            return false;
    } else {
        return false;
    }

    if (read_image(img_path) && parse_cfg(cfg_path)) {
        ecs_attach(0);
        apply_all_regions(0);
        d000_ram = 0;
        bankswitch_active = true;
        ok = true;
    }

    if (!ok)
        bankswitch_unload();
    return ok;
}

void bankswitch_reset(void)
{
    if (bankswitch_active)
        apply_all_regions(0);
}

bool bankswitch_write(int adr, int val)
{
    if (!bankswitch_active)
        return false;

    adr &= 0xFFFF;
    val &= 0xFFFF;

    int r = adr >> 12;
    if ((adr & 0x0FFF) == 0x0FFF && region_paged[r] && (adr & 0xFA50) == (val & 0xFFF0)) {
        apply_region(r, val & 0xF);
        return true;
    }

    /* Writes into cart ROM are ignored, like on the real cart */
    return rom_mapped[adr];
}
