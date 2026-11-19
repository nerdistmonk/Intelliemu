#ifndef INTELLIEMU_BANKSWITCH_H
#define INTELLIEMU_BANKSWITCH_H

#include <stdbool.h>

/* True while a bank-switched cart is mapped into Memory[]. */
extern bool bankswitch_active;

/* Accepts a .cfg, .bin or .int path and loads the cart's mapping and image set. */
bool bankswitch_load(const char *path);
void bankswitch_unload(void);
void bankswitch_reset(void);

/* Returns true when the write was consumed by the bank hardware or ignored as a ROM write. */
bool bankswitch_write(int adr, int val);

#endif
