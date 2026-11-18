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

/* Handles ECS RAM and bank select writes. Returns 1 when the write is fully handled. */
int ecs_write(int adr, int val);

void ECSSerialize(struct ECSserialized *state);
void ECSUnserialize(const struct ECSserialized *state);

#endif
