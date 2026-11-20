# **Intellivision Memory Map & Intellicart Technical Reference**

*`This document is an original technical specification summarizing non-copyrightable hardware memory maps, register definitions, address-decoding behaviors, and toolchain configuration formats. It is released into the public domain (CC0 1.0) and is compatible with GPLv2 and all open-source licenses.`*

# **1\. Intellivision System Memory Map**

## **1.1 Standard Hardware Map**

| Address Range | Access | Timing / Window | Hardware Component |
| :---- | :---- | :---- | :---- |
| `$0000–$003F` | `Read / Write` | `VBlank Period 1` | `STIC Registers` |
| `$0100–$01EF` | `Read / Write` | `Always` | `Scratchpad RAM` |
| `$01F0–$01FF` | `Read / Write` | `Always` | `PSG (Sound) Registers` |
| `$0200–$035F` | `Read / Write` | `Always` | `System RAM` |
| `$1000–$1FFF` | `Read-only` | `Always` | `Executive ROM (EXEC)` |
| `$3000–$37FF` | `Read-only` | `VBlank Period 2` | `Graphics ROM (GROM)` |
| `$3800–$39FF` | `Read / Write` | `VBlank Period 2` | `Graphics RAM (GRAM)` |

## **1.2 Unmapped Bus Behavior**

`The Intellivision uses a bidirectional 16-bit data bus fitted with external pull-up resistors. Any bus device (including the CP1610 CPU) enters a high-impedance state when inactive. Reading an address with no responding device returns $FFFF.`

## **1.3 Incomplete Address Decoding, Aliases, and Model Quirks**

`Due to incomplete address decoding in the STIC and GRAM circuitry (related to System RAM Bus Copy behavior), as well as hardware revisions in the Intellivision II, several additional address ranges exhibit hardware responses:`

| Address Range | Access | Timing / Window | Behavior / Notes |
| :---- | :---- | :---- | :---- |
| `$0040–$007F` | `Read-only` | `VBlank Period 2` | `STIC-responsive (read-only)` |
| `$0360–$03FF` | `Read-only` | `Always` | `Unreliable/garbage reads from incomplete RA-3-9600A decode (Intellivision II only?)` |
| `$0400–$04FF` | `Read-only` | `Always` | `Extended EXEC ROM (Intellivision II only)` |
| `$3A00–$3FFF` | `Read / Write` | `VBlank Period 2` | `GRAM aliases (three 512-word mirrors: $3A00–$3BFF, $3C00–$3DFF, $3E00–$3FFF)` |
| `$4000–$403F` | `Write-only` | `VBlank Period 1` | `STIC register write alias` |
| `$4021` | `Read / Write` | `VBlank Period 1` | `STIC Mode Select alias (reading switches STIC to Color Stack mode)` |
| `$7800–$7FFF` | `Write-only` | `VBlank Period 2` | `GRAM write aliases (four 512-word mirrors)` |
| `$8000–$803F` | `Write-only` | `VBlank Period 1` | `STIC register write alias` |
| `$8021` | `Read / Write` | `VBlank Period 1` | `STIC Mode Select alias (reading switches STIC to Color Stack mode)` |
| `$B800–$BFFF` | `Write-only` | `VBlank Period 2` | `GRAM write aliases (four 512-word mirrors)` |
| `$C000–$C03F` | `Write-only` | `VBlank Period 1` | `STIC register write alias` |
| `$C021` | `Read / Write` | `VBlank Period 1` | `STIC Mode Select alias (reading switches STIC to Color Stack mode)` |
| `$F800–$FFFF` | `Write-only` | `VBlank Period 2` | `GRAM write aliases (four 512-word mirrors)` |

*`Note: Reads from standard STIC or GRAM mirror addresses do not return STIC/GRAM data, except that reading $4021, $8021, or $C021 during VBlank Period 1 triggers Color Stack mode.`*

## **1.4 Cartridge-Available Address Ranges**

`Cartridges may map ROM or RAM into unused portions of the 64K-word address space, subject to peripheral conflicts and hardware aliases:`

| Address Range | Cartridge RAM | Cartridge ROM |
| :---- | :---- | :---- |
| `$0400–$04FF` | `Safe except Intellivision II` | `Safe except Intellivision II` |
| `$0500–$06FF` | `Safe` | `Safe` |
| `$0700–$0CFF` | `Safe if no Intellivoice` | `Safe if no Intellivoice` |
| `$0D00–$0FFF` | `Safe` | `Safe` |
| `$2000–$2FFF` | `Safe if no ECS` | `Safe if no ECS` |
| `$4000–$47FF` | `Safe if no ECS` | `Safe if no ECS` |
| `$4800` | `Safe only if boot ROM is at $7000` | `Safe` |
| `$4801–$4FFF` | `Safe` | `Safe` |
| `$5000–$5014` | `Safe only if boot ROM is at $4800 or $7000` | `Safe` |
| `$5015–$6FFF` | `Safe` | `Safe` |
| `$7000` | `Unsafe (disrupts EXEC boot)` | `Safe if no ECS` |
| `$7001–$77FF` | `Safe if no ECS` | `Safe if no ECS` |
| `$7800–$7FFF` | `Unsafe (GRAM write alias)` | `Safe if no ECS` |
| `$8000–$8FFF` | `Safe (avoid $8000–$803F STIC alias)` | `Safe (avoid $8000–$803F STIC alias)` |
| `$9000–$B7FF` | `Safe` | `Safe` |
| `$B800–$BFFF` | `Unsafe (GRAM write alias)` | `Safe` |
| `$C000–$CFFF` | `Safe (avoid $C000–$C03F STIC alias)` | `Safe (avoid $C000–$C03F STIC alias)` |
| `$D000–$DFFF` | `Safe` | `Safe` |
| `$E000–$EFFF` | `Safe if no ECS` | `Safe if no ECS` |
| `$F000–$F7FF` | `Safe` | `Safe` |
| `$F800–$FFFF` | `Unsafe (GRAM write alias)` | `Safe` |

# **2\. Intellicart Architecture & Bank-Switching**

## **2.1 Dual Address Spaces and Memory Attributes**

`The Intellicart provides 64K x 16-bit (64K words) of onboard RAM in an internal address space kept separate from the Intellivision's 64K-word CPU bus. The cartridge divides the Intellivision address space into 32 regions of 2K words each and assigns independent attributes to every 2K region: Unmodified titles that require 8-bit RAM or ECS-style bank switching are not natively supported on Intellicart hardware.`

* **`Readable (R):`** `Cartridge responds to CPU reads. Readable-only regions emulate ROM.`  
* **`Writeable (W):`** `Cartridge responds to CPU writes. Readable + Writeable emulates RAM; Writeable-only emulates Write-Only Memory (WOM).`  
* **`Bank-switched (B):`** `Region redirects to a dynamically selected base address in Intellicart RAM.`  
* **`Unmapped (none):`** `Neither R nor W is set; the cartridge stays off the bus to avoid colliding with system hardware.`  
* **`Narrow (emulator extension):`** `Supported by emulators such as jzIntv (not native Intellicart hardware) to emulate 8-bit RAM by ignoring writes to the upper byte.`

## **2.2 Fine-Address Trimming**

`To avoid overlapping peripheral holes inside a 2K-word block (for example, the Intellivoice expansion range at $0700–$0CFF inside the $0800–$0FFF block), either or both ends of a 2K range may be trimmed inward in 256-word increments. Trimming cannot create a gap in the middle of a 2K block. Trimmed portions do not respond on the bus.`

## **2.3 Direct Mapping vs. Bank-Switching**

* **`Direct Mapping (Default):`** `A 1:1 correspondence between CPU addresses and Intellicart RAM addresses (e.g., CPU $6000 accesses Intellicart $6000).`  
* **`Bank-Switching:`** `Any 2K-word region marked bank-switched can be pointed to any 256-word-aligned base address in the 64K-word Intellicart RAM.`  
* **`Aliasing Windows:`** `Multiple CPU regions may simultaneously map to the same Intellicart memory with different attributes (for example, a direct-mapped read-only region at $5000–$57FF paired with a bank-switched writeable window at $6000–$67FF or $0E00–$0FFF to modify underlying memory).`

## **2.4 Bank-Switch Register Map (\$0040–\$005F)**

`Bank selection is controlled via 32 write-only registers at $0040–$005F. Writes only affect regions whose Bank-switched attribute is enabled.`

| Register | Controlled 2K Window | Register | Controlled 2K Window |
| :---- | :---- | :---- | :---- |
| `$0040` | `$0000–$07FF` | `$0050` | `$0800–$0FFF` |
| `$0041` | `$1000–$17FF` | `$0051` | `$1800–$1FFF` |
| `$0042` | `$2000–$27FF` | `$0052` | `$2800–$2FFF` |
| `$0043` | `$3000–$37FF` | `$0053` | `$3800–$3FFF` |
| `$0044` | `$4000–$47FF` | `$0054` | `$4800–$4FFF` |
| `$0045` | `$5000–$57FF` | `$0055` | `$5800–$5FFF` |
| `$0046` | `$6000–$67FF` | `$0056` | `$6800–$6FFF` |
| `$0047` | `$7000–$77FF` | `$0057` | `$7800–$7FFF` |
| `$0048` | `$8000–$87FF` | `$0058` | `$8800–$8FFF` |
| `$0049` | `$9000–$97FF` | `$0059` | `$9800–$9FFF` |
| `$004A` | `$A000–$A7FF` | `$005A` | `$A800–$AFFF` |
| `$004B` | `$B000–$B7FF` | `$005B` | `$B800–$BFFF` |
| `$004C` | `$C000–$C7FF` | `$005C` | `$C800–$CFFF` |
| `$004D` | `$D000–$D7FF` | `$005D` | `$D800–$DFFF` |
| `$004E` | `$E000–$E7FF` | `$005E` | `$E800–$EFFF` |
| `$004F` | `$F000–$F7FF` | `$005F` | `$F800–$FFFF` |

## **2.5 Address Translation Formula & Startup Requirements**

`Each bank-select register latches the lower 8 bits written to it (upper 8 bits are ignored). That 8-bit value sets the 256-word starting offset in Intellicart RAM for the corresponding 2K window:`

```
uint16_t target_base = (bank_select & 0x00FF) << 8;
uint16_t icart_addr  = (intv_addr & 0x07FF) + target_base;
```

*`Example:`* `With $6000–$67FF marked bank-switched, writing $3F to register $0046 maps CPU $6000–$67FF to Intellicart $3F00–$46FF. Accessing CPU $6123 resolves to ($6123 & $07FF) + $3F00 = $4023.`

**`Startup Caveats:`**

* `Bank-switch registers power on in an undefined state.`  
* `Never place startup/boot code in a bank-switched region, and do not mark $4800, $5000, or $7000 as bank-switched (doing so can disrupt EXEC boot detection).`  
* `Always initialize bank-switch registers explicitly before reading from or writing to any bank-switched window.`

# **3\. Toolchain & Configuration Reference**

## **3.1 AS1600 Assembler Directives**

`as1600 encodes Intellicart memory attributes directly into .ROM output (or .CFG output when building BIN+CFG pairs). By default, assembled code is marked Readable.`

* **`Separate Load Address (ORG):`** `ORG $5000, $1000 sets the CPU symbol address to $5000 and the Intellicart load address to $1000. When CPU and load addresses differ, as1600 defaults to no memory attributes for that block.`  
* **`Attribute Mode String (ORG):`** `ORG $D000, $D000, "+RW" applies the specified attribute flags to the Intellicart address range for subsequent assembled data until the next ORG.`  
* **`Explicit Range Attributes (MEMATTR):`** `MEMATTR $D000, $D7FF, "+RWB" sets or modifies (delta or absolute) attributes on a specified address range.`

## **3.2 BIN+CFG Format (`bin2rom` / `rom2bin`)**

`The bin2rom utility converts a raw .BIN file and .CFG map into an Intellicart .ROM image using four recognized sections (all other sections are ignored):`

```
; Map BIN word range $xxxx-$yyyy to Intellicart address $zzzz
; and mark the range direct-mapped Readable:
[mapping]
$xxxx - $yyyy = $zzzz

; Load BIN word range $xxxx-$yyyy to Intellicart address $zzzz
; without modifying memory attributes:
[preload]
$xxxx - $yyyy = $zzzz

; Mark Intellivision address range as 8-bit or 16-bit RAM or WOM
; (Note: Intellicart hardware does not support 8-bit RAM/WOM;
; INTVPC and the Intellicart loader do not recognize WOM):
[memattr]
$xxxx - $yyyy = RAM 8
$xxxx - $yyyy = RAM 16
$xxxx - $yyyy = WOM 8
$xxxx - $yyyy = WOM 16

; Mark Intellivision address range as bank-switched:
[bankswitch]
$xxxx - $yyyy
```

**`Compatibility Notes:`**

* `If conflicting attributes are declared within the same 2K-word block, bin2rom applies the superset of the requested attributes.`  
* `When converting .ROM back to BIN+CFG, rom2bin (and as1600) emits WOM ranges as RAM for broader loader/emulator compatibility.`

