/*
	This file is part of FreeIntv.

	FreeIntv is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	FreeIntv is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with FreeIntv; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "psg.h"
#include "memory.h"
#include "ecs.h"

int Volume[16] = { 0, 92, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 10922 };

int Envelope_Shift[4] = {8, 2, 1, 0};

// Volume levels assigned to each channel from PSG registers
// Register offsets are relative to p->base, so each chip reads its own block
#define REG(p, off) (Memory[(p)->base + (off)])

#define VolA(p)    (REG(p, 0x0B) & 0x0F)
#define VolB(p)    (REG(p, 0x0C) & 0x0F)
#define VolC(p)    (REG(p, 0x0D) & 0x0F)

// Envelope shifts for channels (6-bit variations only)
#define EnvA(p)    ((REG(p, 0x0B) >> 4) & 0x03)
#define EnvB(p)    ((REG(p, 0x0C) >> 4) & 0x03)
#define EnvC(p)    ((REG(p, 0x0D) >> 4) & 0x03)

// Detect Tone enabled for this channel (0- enabled, 1- disabled)
#define ToneA(p)   ((REG(p, 0x08) & 0x01) != 0)
#define ToneB(p)   ((REG(p, 0x08) & 0x02) != 0)
#define ToneC(p)   ((REG(p, 0x08) & 0x04) != 0)
           
// Detect Noise enabled for this channel (0- enabled, 1- disabled)
#define NoiseA(p)  ((REG(p, 0x08) & 0x08) != 0)
#define NoiseB(p)  ((REG(p, 0x08) & 0x10) != 0)
#define NoiseC(p)  ((REG(p, 0x08) & 0x20) != 0)

// Envelope type
#define EnvFlags(p)    (REG(p, 0x0A) & 0x0F)

int PSGBufferSize;
int16_t PSGBuffer[7467];
int PSGBufferPos;

int Ticks; // CPU cycles not yet processed

static struct PSGchip psg[2]; // psg[0] is PSG1, psg[1] is ECS PSG2

void PSGSerialize(struct PSGserialized *all)
{
    all->PSGBufferSize = PSGBufferSize;
    memcpy(all->PSGBuffer, PSGBuffer, sizeof(PSGBuffer));
    all->PSGBufferPos = PSGBufferPos;
    all->Ticks = Ticks;
    memcpy(all->chip, psg, sizeof(psg));
}

void PSGUnserialize(const struct PSGserialized *all)
{
    PSGBufferSize = all->PSGBufferSize;
    memcpy(PSGBuffer, all->PSGBuffer, sizeof(PSGBuffer));
    PSGBufferPos = all->PSGBufferPos;
    Ticks = all->Ticks;
    memcpy(psg, all->chip, sizeof(psg));
}

static void readRegisters(struct PSGchip *p)
{
	p->ChA = (REG(p, 0x00) & 0xFF) | ((REG(p, 0x04) & 0x0F)<<8);
	p->ChB = (REG(p, 0x01) & 0xFF) | ((REG(p, 0x05) & 0x0F)<<8);
	p->ChC = (REG(p, 0x02) & 0xFF) | ((REG(p, 0x06) & 0x0F)<<8);
 
    p->ChA = p->ChA + (0x1000 * (p->ChA==0)); // a Channel Period value of 0
    p->ChB = p->ChB + (0x1000 * (p->ChB==0)); // indicates a value of 0x1000
    p->ChC = p->ChC + (0x1000 * (p->ChC==0));

    p->NoiseP = (REG(p, 0x09) & 0x1F)<<1;

    // a Noise Period of 0 indicates a period of 0x40
    p->NoiseP = p->NoiseP + (0x40 * (p->NoiseP==0));

    p->EnvP = ((REG(p, 0x03) & 0xFF) | ((REG(p, 0x07) & 0xFF)<<8))<<1;

    // an Envelope Period of 0 indicates a period of 0x20000
    p->EnvP = p->EnvP + (0x20000 * (p->EnvP==0));

	// Envelope Flags
	p->EnvContinue = (EnvFlags(p)>>3) & 0x01;
	p->EnvAttack = (EnvFlags(p)>>2) & 0x01;
	p->EnvAlternate = (EnvFlags(p)>>1) & 0x01;
	p->EnvHold = EnvFlags(p) & 0x01;
}

void PSGInit()
{
	int i;
	PSGBufferSize = 7467; // set in psg.h

	psg[0].base = 0x01F0;
	psg[1].base = 0x00F0;

	for(i = 0; i < 2; i++)
	{
		struct PSGchip *p = &psg[i];
		p->OutA = 0; // tone generator outputs
		p->OutB = 0;
		p->OutC = 0;
		p->OutN = 0x10004; // noise output
		p->OutE = 0; // envelope output
		p->CountA = 0; // tone generator countdowns
		p->CountB = 0;
		p->CountC = 0;
		p->CountN = 0; // noise generator countdown
		p->CountE = 0; // envelope countdown
		readRegisters(p);
	}
}

void PSGFrame()
{
	PSGBufferPos = 0;
 #if 0  // Debugging
    {
        fprintf(stderr, "%04x %04x %04x %02x %02x %02x\n", ChA, ChB, ChC, VolA, VolB, VolC);
    }
 #endif
}

int psg_masks[16] = {
    0xff, 0xff, 0xff, 0xff,
    0x0f, 0x0f, 0x0f, 0xff,
    0xff, 0x1f, 0x0f, 0x3f,
    0x3f, 0x3f, 0xff, 0xff,
};

static void psgNotify(struct PSGchip *p, int adr, int val)
{
    Memory[adr] &= psg_masks[adr - p->base];
	readRegisters(p);
    // Note: updating frequencies doesn't reset counters in real chip
    //       (otherwise sound glitch happens in games)

	// Envelope properties Trigger (write only register)
	if (adr == p->base + 0x0A)  
	{ 
		p->CountE = p->EnvP;
		p->StepE = 0;

		if (p->EnvAttack) // attack __/|/|/|___
		{
			p->OutE = 0;
			p->StepE = 1;
		}
		else
		{
			p->OutE = 15;
			p->StepE = -1;
		}
	}
}

void PSGNotify(int adr, int val) // PSG1 registers 0x1F0-0x1FD (called from writeMem)
{
	psgNotify(&psg[0], adr, val);
}

void PSG2Notify(int adr, int val) // ECS PSG2 registers 0xF0-0xFD (called from writeMem)
{
	psgNotify(&psg[1], adr, val);
}

// Advances one PSG chip by one clock and returns its unscaled channel sum
static int psgStep(struct PSGchip *p)
{
	int a, b, c;

	p->CountA--;
	p->CountB--;
	p->CountC--;
	p->CountN--;
	p->CountE--;

	/* ************** Generate Sample ************** */

	p->OutA = p->OutA ^ (p->CountA<=0); // Tone Generators
	p->OutB = p->OutB ^ (p->CountB<=0);
	p->OutC = p->OutC ^ (p->CountC<=0);

	// http://spatula-city.org/~im14u2c/intv/jzintv-1.0-beta3/doc/programming/psg.txt
	if(p->CountE==0) // Envelope Generator
	{
		p->CountE = p->EnvP; // reset countdown
		p->OutE = p->OutE + p->StepE; // step up, step down, or hold

		if(p->StepE != 0 && (p->OutE>15 || p->OutE<0)) // we've reached the top or bottom
		{
			if(p->EnvHold)
			{
				p->StepE = 0; // stop changing (hold volume)
				if(p->EnvAlternate) // alternate & hold  1011 1111
				{
					p->OutE = 15 * (p->EnvAttack==0);
				}
				else // hold at 0 (1001) or 15 (1101)
				{
					p->OutE = 15 * (p->EnvAttack==1);
				}
			}
			else
			{
				if(p->EnvAlternate) // triange waves__/\/\/\__ 1010  \/\/\/\___ 1110
				{
					p->StepE = p->StepE * -1;    // Swap step direction
					p->OutE = (p->OutE + p->StepE) & 0x0F;
				}
				else // saw-tooth waves __|\|\|\__ 1000 ___/|/|/|___ 1100
				{
					p->OutE = 15 * (p->EnvAttack==0);
				}
			}
			// Anything without continue flag set holds at 0
			if(p->EnvContinue==0)
			{
				p->OutE = 0;
				p->StepE = 0;
			}
		}
	}

	// http://wiki.intellivision.us/index.php?title=PSG
	// noise = (noise >> 1) ^ ((noise & 1) ? 0x14000 : 0);
	// The wiki is wrong as MAME says the LFSR noise is
	// bit 0 + bit 3 so the correct mask is 0x10004
	if(p->CountN<=0)
	{
		p->CountN = p->NoiseP;
		p->OutN = (p->OutN >> 1) ^ ((p->OutN & 1) * 0x10004); // Noise Generator
	}

	// http://wiki.intellivision.us/index.php?title=PSG
	// channel_output = (noise_enable OR noise_generator_output) AND (tone_enable OR tone_generator_output)
	a = (NoiseA(p) | (p->OutN & 1)) & (ToneA(p) | p->OutA); // Generate Sample for each channel
	b = (NoiseB(p) | (p->OutN & 1)) & (ToneB(p) | p->OutB);
	c = (NoiseC(p) | (p->OutN & 1)) & (ToneC(p) | p->OutC);

	// Adjust amplitude (Volume / Envelope)
	a = a * ( (Volume[VolA(p)] * (EnvA(p)==0)) | (Volume[p->OutE >> Envelope_Shift[EnvA(p)]]) );
	b = b * ( (Volume[VolB(p)] * (EnvB(p)==0)) | (Volume[p->OutE >> Envelope_Shift[EnvB(p)]]) );
	c = c * ( (Volume[VolC(p)] * (EnvC(p)==0)) | (Volume[p->OutE >> Envelope_Shift[EnvC(p)]]) );

	/* ********************************************* */

	p->CountA += p->ChA * (p->CountA<=0); // reset countdowns when they reach 0
	p->CountB += p->ChB * (p->CountB<=0);
	p->CountC += p->ChC * (p->CountC<=0);

	return a + b + c;
}

void PSGTick(int ticks) // adds 1 sound sample per 4 cpu cycles to the buffer
{
	int sample;

	Ticks = Ticks + ticks;

	while(Ticks >= 4)
	{
		Ticks -= 4;

		sample = psgStep(&psg[0]);
		if(ecs_enabled)
		{
			// PSG2 is summed into the same output; clamp so loud peaks don't wrap
			sample += psgStep(&psg[1]);
			if(sample > 32767) sample = 32767;
			else if(sample < -32768) sample = -32768;
		}

		PSGBuffer[PSGBufferPos] = sample; // write sample to buffer

		PSGBufferPos++;
		PSGBufferPos = PSGBufferPos * (PSGBufferPos < 7467); // wrap to beginning
	}
}

