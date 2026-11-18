/*
	ECS (Entertainment Computer System) emulation.

	ECS.bin is three 4K-word ROM chunks, each stored as big-endian 16-bit words:
	  file $0000 -> $2000 window, shown when its bank register is 1
	  file $2000 -> $7000 window, shown when its bank register is 0
	  file $4000 -> $E000 window, shown when its bank register is 1
	A window that is not selected shows the normal memory underneath.
*/
#include <stdio.h>

#include "ecs.h"
#include "memory.h"

#ifdef __LIBRETRO__
#include <streams/file_stream.h>
#endif

#define ECS_RAM_START 0x4040 /* $4000-$403F alias the STIC */
#define ECS_RAM_END   0x47FF

int ecs_enabled = 0;

static int ecs_loaded = 0;
static int ecs_bank[3];
static unsigned int ecs_rom[3][0x1000];

static const int ecs_base[3]     = { 0x2000, 0x7000, 0xE000 };
static const int ecs_page[3]     = { 1, 0, 1 };
static const int ecs_bank_adr[3] = { 0x2FFF, 0x7FFF, 0xEFFF };

int ecs_load(const char *path)
{
	unsigned char buf[ECS_ROM_SIZE];
	size_t n;
	int c, i;

#ifdef __LIBRETRO__
	RFILE *fp;
	if((fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE)) == NULL)
		return 0;
	n = (size_t)filestream_read(fp, buf, sizeof(buf));
	filestream_close(fp);
#else
	FILE *fp;
	if((fp = fopen(path, "rb")) == NULL)
		return 0;
	n = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);
#endif

	if(n != ECS_ROM_SIZE)
	{
		printf("[ERROR] [FREEINTV] ECS BIOS must be %d bytes: %s\n", ECS_ROM_SIZE, path);
		return 0;
	}

	for(c = 0; c < 3; c++)
	{
		for(i = 0; i < 0x1000; i++)
		{
			const unsigned char *p = &buf[c * 0x2000 + i * 2];
			ecs_rom[c][i] = (p[0] << 8) | p[1];
		}
	}

	ecs_loaded = 1;
	printf("[INFO] [FREEINTV] Succeeded loading ECS BIOS from: %s\n", path);
	return 1;
}

void ecs_reset(void)
{
	int w;
	for(w = 0; w < 3; w++)
		ecs_bank[w] = 0;
}

void ecs_attach(int wanted)
{
	int i;

	if(wanted && !ecs_loaded)
		printf("[WARNING] [FREEINTV] This cartridge needs ECS.bin, which was not found\n");

	ecs_enabled = (wanted && ecs_loaded) ? 1 : 0;

	for(i = ECS_RAM_START; i <= ECS_RAM_END; i++)
		Memory[i] = ecs_enabled ? 0 : 0xFFFF;

	ecs_reset();
}

int ecs_read(int adr, int *val)
{
	int w, off;
	for(w = 0; w < 3; w++)
	{
		off = adr - ecs_base[w];
		if(off >= 0 && off < 0x1000 && ecs_bank[w] == ecs_page[w])
		{
			*val = ecs_rom[w][off];
			return 1;
		}
	}
	return 0;
}

int ecs_write(int adr, int val)
{
	int w;

	if(adr >= ECS_RAM_START && adr <= ECS_RAM_END)
	{
		Memory[adr] = val & 0xFF;
		return 1;
	}

	/* Return 0 so a bank write still reaches the normal write path, which also stores to $7FFF GRAM */
	for(w = 0; w < 3; w++)
	{
		if(adr == ecs_bank_adr[w] && (val & 0x0FF0) == 0x0A50)
			ecs_bank[w] = val & 0x0F;
	}
	return 0;
}

void ECSSerialize(struct ECSserialized *state)
{
	int w;
	state->enabled = ecs_enabled;
	for(w = 0; w < 3; w++)
		state->bank[w] = ecs_bank[w];
}

void ECSUnserialize(const struct ECSserialized *state)
{
	int w;
	ecs_enabled = state->enabled && ecs_loaded;
	for(w = 0; w < 3; w++)
		ecs_bank[w] = state->bank[w];
}
