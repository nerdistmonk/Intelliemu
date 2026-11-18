#ifndef ECS_H
#define ECS_H
/*
	Entertainment Computer System (ECS): 12K of bankswitched ROM at $2000, $7000 and $E000,
	8-bit RAM at $4040-$47FF, and a second PSG at $F0-$FF.
*/

#define ECS_ROM_SIZE 24576

/* 1 while the running cartridge uses the ECS and ECS.bin is loaded */
extern int ecs_enabled;

struct ECSserialized
{
	int enabled;
	int bank[3];
};

/* Loads ECS.bin. Returns 1 on success. */
int ecs_load(const char *path);

/* Enables the ECS for a cartridge when wanted and ECS.bin is loaded. Also clears ECS RAM. */
void ecs_attach(int wanted);

/* Selects page 0 on every ECS window, as after RESET. */
void ecs_reset(void);

/* Returns 1 and sets *val when an ECS ROM window is visible at adr. */
int ecs_read(int adr, int *val);

/* Turns the ECS keyboard matrix on or off. Turning it off releases every key. */
void ecs_keyboard_enable(int on);

/* 1 while the ECS keyboard matrix replaces the plain $FE/$FF reads. */
int ecs_keyboard_active(void);

/* cols[c] bit r is set while the key at row r, column c is held. Rows are 0-6, columns 0-7. */
void ecs_keyboard_set_matrix(const unsigned char cols[8]);

/* Returns the value read from port $FE or $FF through the keyboard matrix. */
int ecs_keyboard_port_read(int adr);

/* Handles ECS RAM and bank select writes. Returns 1 when the write is fully handled. */
int ecs_write(int adr, int val);

void ECSSerialize(struct ECSserialized *state);
void ECSUnserialize(const struct ECSserialized *state);

#endif
