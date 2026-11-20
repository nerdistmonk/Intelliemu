# ECS Keyboards Hardware & Scanning Reference

*This document summarizes non-copyrightable hardware interfaces, register behavior, matrix-scanning constraints, and verified errata for the Intellivision Entertainment Computer System (ECS) keyboards. Released into the public domain (CC0 1.0) for compatibility with GPLv2 and open-source projects.*

## 1\. PSG Hardware & Port Mapping

* **Sound Chip:** The ECS provides a second Programmable Sound Generator (AY-3-8917) mapped to `$00F0–$00FF` (unlike the Master Component's AY-3-8914 at `$01F0–$01FF`).  
* **Bidirectional I/O Ports:** The AY-3-8917 exposes two 8-bit bidirectional ports:  
  * `$00FE` (left connector): Connects to the **row** lines in the keyboard matrix.  
  * `$00FF` (right connector): Connects to the **column** lines in the keyboard matrix.  
* **Direction Control (`$00F8`):** Bits 6 and 7 of register `$00F8` (R7 enable/I/O control) set port direction for `$00FE` and `$00FF` respectively (`0` \= input, `1` \= output). Each 8-bit port is wholly input or wholly output (no per-bit direction mixing). The lower bits (`0–5`) control tone/noise enables and should be preserved during read-modify-write updates.  
* **Electrical Characteristics:** Internal pull-up resistors cause undriven/input lines to read as `1` (`$FF` when all bits are high). The ECS includes sixteen 2K-ohm current-limiting resistors (two per port line) for protection. Active-low scanning drives a selected output line to `0` and detects active inputs as `0`.

## 2\. DB9 Connector Pinout & Continuity Corrections

Physical inspection and continuity testing later in the thread corrected earlier pin-numbering assumptions:

* **DB9 Pin Layout (viewed from above the ECS):**  
  * Top row (left to right): `1  2  3  4  5`  
  * Bottom row (left to right): `6  7  8  9`  
* **Signal Mapping:**  
  * Pin 1: Bit 3  
  * Pin 2: Bit 2  
  * Pin 3: Bit 1  
  * Pin 4: Bit 0  
  * Pin 5: GND  
  * Pin 6: Bit 7  
  * Pin 7: Bit 6  
  * Pin 8: Bit 5  
  * Pin 9: Bit 4  
* **Correction Note:** Earlier notes referencing a cable pinout and earlier `ECScable` documentation had a right-to-left mirror/numbering discrepancy; continuity verification on the ECS hardware and cable confirmed the mapping above.

## 3\. Alphanumeric Keyboard Matrix & Ghosting/Masking

* **Matrix Layout:** 49 physical keys arranged across a matrix without isolation diodes. Both `Shift` keys share a single matrix position (forming a 48-position logical matrix). Row 6 contains only `Shift`, Row 7 is unused, and Row 0 / Column 7 has no key.  
* **Whole-Port Drive Masking ("Son of Ghosting"):** Because an output port drives all 8 lines simultaneously (`0` on the active line, `1` on all other 7 lines), a driven `1` overrides a key pulling a line low (`0`). Pressing two keys in the same column masks both out during a row-driven scan (`$00FE` output, `$00FF` input).  
* **Transposed Scanning & Shift:** Swapping output and input roles (`$00FF` output driving columns, `$00FE` input reading rows) resolves same-column conflicts and allows reading `Shift` (Row 6\) alongside keys in columns shared with `Shift` (`L`, `J`, `G`, `D`, `A`), which standard row scanning cannot upper-case when `Shift` is held. Transposing shifts same-column masking to same-row key pairs.  
* **Control (`CTL`) Key Behavior:** `CTL` shares a row with `Space`, `Down Arrow`, `Up Arrow`, `Q`, `1`, `Right Arrow`, and `A`, and shares a column with `RTN`, `O`, `U`, `T`, and `E`. Handling `CTL` cleanly alongside all row/column peers requires combining normal and transposed scans; simpler column-oriented scanners typically treat `CTL` as a modifier without full row-5/col-6 conflict resolution.  
* **Verified Shifted Table Corrections:** Later thread follow-up noted that the less-than character (`<`) had been replaced with `_` in an earlier forum post due to HTML parsing, and identified two missing symbols in the shifted decoder table: `<` (less-than) and `'` (apostrophe).  
* **Debouncing:** Because matrix scanning is slow relative to switch bounce and each key is a single contact (unlike dual-contact mylar controller switches), heavy pre-decode debouncing is generally unnecessary beyond avoiding immediate re-scan chatter.

## 4\. Synthesizer Keyboard

* **Matrix & Diodes:** 49-key (4-octave, C-to-C) keyboard matrix equipped with isolation diodes, avoiding ghosting and masking conflicts when multiple keys are held simultaneously.  
* **Note Index Mapping:** Notes map directly by matrix position as `row * 8 + col` (lowest note at Row 0, Col 0; highest at Row 6, Col 0).  
* **State Tracking:** Multi-key simultaneous presses/releases are tracked cleanly by comparing a per-row bitmap against the previous scan state (e.g., via `XOR`) to emit key-up and key-down transitions (emitting releases before presses helps avoid voice/channel allocation collisions).

## 5\. NTSC vs. PAL Detection Notes

* **No Reliable EXEC/ROM Flag:** There is no consistent EXEC ROM or user-visible hardware register difference across all models (such as Intellivision I units) to distinguish NTSC (60 Hz) from PAL (50 Hz).  
* **Timing Measurement:** Standard detection measures elapsed time/loop iterations between VBLANK interrupts (approximately 14,932 cycles per frame on NTSC vs. roughly 19,900+ cycles on PAL at their respective frame rates). Polling GROM visibility after enabling display is faster but less portable across emulators that do not model GROM bus isolation.