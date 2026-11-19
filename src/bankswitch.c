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
#define BANK_PAGES      32

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
static bool paged_mapped[0x10000];
static bool region_paged[BANK_REGIONS];
static unsigned region_page[BANK_REGIONS];

/* Plain (non-PAGE) spans flattened by Memory address. Intellicart bank
 * registers select 2K windows of this view. */
static uint16_t cart_flat[0x10000];
static uint16_t bank_tbl[BANK_PAGES];
/* Bank register gating. A cfg with a [bankswitch] section uses jzIntv's
 * B-flag model: bank_pg marks the 256-word pages it lists. A cfg without
 * one keeps the legacy rule: any plain span enables the registers. */
static bool bank_sec = false;
static bool bsw_any = false;
static bool bank_pg[0x10000 >> 8];
static bool plain_any = false;

static bool bank_regs_on(void)
{
    return bank_sec ? bsw_any : plain_any;
}

static bool plain_mapped(unsigned a)
{
    return rom_mapped[a] && !paged_mapped[a];
}

static bool bank_write_ok(unsigned target)
{
    return bank_sec ? bank_pg[target >> 8] : plain_mapped(target);
}

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

/* Fills region r from its selected PAGE spans: unselected paged addresses
 * read 0xFFFF. Plain spans are handled by apply_bank_page(). Addresses that
 * no span covers (RAM, GRAM, I/O) are never touched. */
static void apply_region(int r, unsigned page)
{
    unsigned base = (unsigned)r << 12;
    unsigned end = base + 0x1000;

    for (unsigned a = base; a < end; a++)
        if (paged_mapped[a])
            Memory[a] = 0xFFFF;

    for (int i = 0; i < nspans; i++) {
        const bank_span_t *s = &spans[i];
        if (!s->paged || s->page != page)
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

/* Refills one 2K window of plain spans, selected by its bank register */
static void apply_bank_page(unsigned page)
{
    unsigned base = page << 11;

    for (unsigned a = base; a < base + 0x800; a++)
        if (plain_mapped(a))
            Memory[a] = cart_flat[(a + bank_tbl[page]) & 0xFFFF];
}

static void apply_all(void)
{
    for (int r = 0; r < BANK_REGIONS; r++)
        if (region_paged[r])
            apply_region(r, region_page[r]);
    for (unsigned p = 0; p < BANK_PAGES; p++)
        apply_bank_page(p);
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
        if (paged) {
            paged_mapped[addr] = true;
            region_paged[addr >> 12] = true;
        } else {
            cart_flat[addr] = image_word(s + i);
        }
    }
    if (!paged)
        plain_any = true;
    return true;
}

/* Preload fills cart_flat only, so the words are present for bank windows
 * but not mapped into Memory, as with jzIntv's [preload]. */
static bool add_preload(unsigned s, unsigned e, unsigned a)
{
    if (e < s || e >= image_words)
        return false;

    unsigned len = e - s + 1;
    if (a + len > 0x10000)
        return false;

    for (unsigned i = 0; i < len; i++)
        cart_flat[a + i] = image_word(s + i);
    return true;
}

static bool parse_cfg(const char *cfg_path)
{
    RFILE *f = open_read(cfg_path);
    char line[512];
    bool in_mapping = false;
    bool in_preload = false;
    bool in_banksw = false;
    bool ok = true;

    if (!f)
        return false;

    while (ok && filestream_gets(f, line, sizeof(line))) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '[') {
            in_mapping = strncmp(p, "[mapping]", 9) == 0;
            in_preload = strncmp(p, "[preload]", 9) == 0;
            in_banksw = strncmp(p, "[bankswitch]", 12) == 0;
            if (in_banksw)
                bank_sec = true;
            continue;
        }
        if (in_preload && *p == '$') {
            unsigned s, e, a;
            if (sscanf(p, "$%x - $%x = $%x", &s, &e, &a) != 3) {
                ok = false;
                break;
            }
            ok = add_preload(s, e, a);
            continue;
        }
        if (in_banksw && *p == '$') {
            unsigned s, e;
            if (sscanf(p, "$%x - $%x", &s, &e) != 2 || e < s || e >= 0x10000) {
                ok = false;
                break;
            }
            for (unsigned pg = s >> 8; pg <= e >> 8; pg++)
                bank_pg[pg] = true;
            bsw_any = true;
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

static int count_paged_spans(void)
{
    int n = 0;
    for (int i = 0; i < nspans; i++)
        if (spans[i].paged)
            n++;
    return n;
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
    bank_sec = false;
    bsw_any = false;
    plain_any = false;
    memset(bank_pg, 0, sizeof(bank_pg));
    memset(rom_mapped, 0, sizeof(rom_mapped));
    memset(paged_mapped, 0, sizeof(paged_mapped));
    memset(region_paged, 0, sizeof(region_paged));
    memset(region_page, 0, sizeof(region_page));
    memset(bank_tbl, 0, sizeof(bank_tbl));
    memset(cart_flat, 0xFF, sizeof(cart_flat));
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
        apply_all();
        d000_ram = 0;
        bankswitch_active = true;
        ok = true;

        /* jzIntv's Intellicart path skips PAGE spans; we apply them from the cfg */
        int npaged = count_paged_spans();
        if (npaged > 0)
            printf("[INFO] [FREEINTV] Bankswitch cfg: %d PAGE span(s) applied as ECS paging\n", npaged);
    }

    if (!ok)
        bankswitch_unload();
    return ok;
}

void bankswitch_reset(void)
{
    if (bankswitch_active) {
        memset(region_page, 0, sizeof(region_page));
        memset(bank_tbl, 0, sizeof(bank_tbl));
        apply_all();
    }
}

bool bankswitch_write(int adr, int val)
{
    if (!bankswitch_active)
        return false;

    adr &= 0xFFFF;
    val &= 0xFFFF;

    int r = adr >> 12;
    if ((adr & 0x0FFF) == 0x0FFF && region_paged[r] && (adr & 0xFA50) == (val & 0xFFF0)) {
        region_page[r] = val & 0xF;
        apply_region(r, region_page[r]);
        return true;
    }

    /* Intellicart bank registers, as in jzIntv. Writes to blocks that are
     * not enabled for banking are dropped rather than reaching RAM. */
    if (bank_regs_on() && adr >= 0x0040 && adr <= 0x005F) {
        unsigned target = ((adr & 0xF) << 12) | ((adr & 0x10) << 7);
        if (bank_write_ok(target)) {
            unsigned page = target >> 11;
            bank_tbl[page] = ((val << 8) - (target & ~0x0700)) & 0xFFFF;
            apply_bank_page(page);
        }
        return true;
    }

    /* Writes into cart ROM are ignored, like on the real cart */
    return rom_mapped[adr];
}
