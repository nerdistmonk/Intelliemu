# freeintv with Onscreen Interactive Keypad Overlays
freeintv is a libretro emulation core for the Mattel Intellivision designed to be compatible with joypads from the SNES era forward even if they originally required a number pad.

**Current Version**: Enhanced with onscreen interactive keypad overlays for Android, Windows, and Linux (November 24, 2025)

## Authors & Contributors

freeintv was created by David Richardson.
The PSG and STIC emulation was made closer to hardware and optimized by Oscar Toledo G. (nanochess), who also added save states.

The Intellivoice code has been contributed by Joe Zbiciak (author of jzintv), and adapted by Oscar Toledo G. (nanochess)

**Onscreen Overlay Enhancement**: Onscreen interactive keypad overlays with touchscreen (Android) and mouse (Windows/Linux) support (November 2025) by Jason Carr

<div style="margin: 18px 0;"><img width="1291" height="727" alt="FreeIntv Screenshot 1" src="screenshots/Screenshot 2025-11-24 194829.png" style="border:2px solid #FFD700; border-radius:6px;" /></div>
<div style="margin: 18px 0;"><img width="1372" height="770" alt="FreeIntv Screenshot 2" src="screenshots/Screenshot 2025-11-24 194901.png" style="border:2px solid #FFD700; border-radius:6px;" /></div>
<div style="margin: 18px 0;"><img width="1365" height="765" alt="FreeIntv Screenshot 3" src="screenshots/Screenshot 2025-11-24 194935.png" style="border:2px solid #FFD700; border-radius:6px;" /></div>

## Building

Linux only, using CMake 3.16+ and GCC. The standalone frontend needs SDL3 development files (tested with SDL 3.4.20; if your distribution does not package SDL3, build it from source).

```sh
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release   # configure
cmake --build release                             # builds release/freeintv_libretro.so and release/intelliemu
```

To build only the libretro core without SDL3, configure with `-DINTELLIEMU_STANDALONE=OFF`. If CMake cannot find SDL3, add `-DCMAKE_PREFIX_PATH=<prefix>` to the configure step.

## Installation

**Step 1: Install freeintv Core**
- Place the built `freeintv_libretro.so` (see [Building](#building)) into RetroArch's `cores/` directory

**Step 2: Install the Core Info File**
- Copy `freeintv_libretro.info` to RetroArch's `info/` directory
- ⚠️ The filename must match the core filename exactly (excluding the file extension)

**Step 3: Add BIOS Files**
- Place the two required Intellivision BIOS files in RetroArch's `system/` directory:
  - `exec.bin` (Executive ROM)
  - `grom.bin` (Graphics ROM)

**Step 4: Launch Games**
- Load an Intellivision ROM through RetroArch using the freeintv core

## Standalone Emulator (SDL3)

`standalone/` contains an SDL3 frontend that loads the freeintv core directly, without RetroArch.

After building (see [Building](#building)):

```sh
./release/intelliemu -s system Game.int
```

The frontend loads `freeintv_libretro.so` from the build directory by default (the absolute path is baked in at build time). Use `-c PATH` to load a different core. Run `intelliemu --help` for all options.

- **BIOS:** `exec.bin` and `grom.bin` are read from the `system_dir` setting (default: the current working directory). `-s DIR` overrides it for one run.
- **Save states:** written next to the ROM, as `<ROM file name without extension>.state<slot>`.
- **Settings:** stored in `~/.config/Intelliemu/config.ini` (`$XDG_CONFIG_HOME/Intelliemu/` if set). The file is created on first run. Keys:
  - `scale`: initial window scale, 1-16. `-x N` overrides it for one run.
  - `system_dir`: folder containing `exec.bin` and `grom.bin`.
  - `fullscreen`, `muted`: `1` or `0`. Changed from F11 and the Audio menu, and saved immediately.
  - `state_slot`: save state slot, 1-9. Changed with F9 and the State menu, and saved immediately.
- **Recent ROMs:** stored in `recent.txt` in the same config directory.
- Command-line options apply to that run only and are never written to `config.ini`.

| Key | Action |
| --- | --- |
| F1 / F5 | Save / load state in the current slot |
| F9 | Select next state slot (1-9) |
| F10 | Reset |
| F11 | Toggle fullscreen |
| Esc | Quit |

Save states are written next to the ROM as `<ROM name without extension>.state<N>`.

Keyboard (player 1): arrows = D-pad, Z/X/C/V = A/B/Y/X, Q/E = L/R, Return = Start, Backspace = Select, 0-9 / `[` / `]` = keypad.

## Enabling Onscreen Interactive Overlays

The freeintv core features onscreen interactive keypad overlays that display custom controller graphics directly on screen, allowing touch or mouse input.

### To Enable the Feature:

1. **Launch a game** using the FreeIntv core
2. **Open the Quick Menu** (usually `Hotkey + X` on your controller, or through RetroArch's menu)
3. **Navigate to**: Options → Core Options
4. Enable the overlay feature
5. **Close the menu** (RESTART to take effect)
6. **Revert to fullscreen** follow same instructions but disable core option, and restart to take effect.

The onscreen interactive overlay will now be active, allowing you to touch or click buttons directly on screen!

## License
The FreeIntv core is licensed under GPLv2+. More information at https://github.com/libretro/FreeIntv/blob/master/LICENSE

## BIOS
FreeIntv requires two Intellivision BIOS files to be placed in the libretro 'system' folder:

| Function | Filename* | MD5 Hash |
| --- | --- | --- | 
| Executive ROM | `exec.bin`  | `62e761035cb657903761800f4437b8af` |
| Graphics ROM | `grom.bin` | `0cd5946c6473e42e8e4c2137785e427f` |

* BIOS filenames are case-sensitive

## Entertainment Computer System
FreeIntv does not currently support Entertainment Computer System (ECS) functionality. Contributions to the code are welcome!

## Game Overlays

Mattel Intellivision games were often meant to be played with game-specific cards overlaid on the numeric keypad. These overlays convey information which can be very useful in gameplay. Images of a limited selection of Intellivision titles are available at: http://www.intellivisionlives.com/bluesky/games/instructions.shtml

### Overlay Setup Instructions

To use custom game overlays with the touchscreen UI on Android or mouse input on Windows/Linux:

**Folder Location:**
- Create a folder named `freeintv_overlays` in the same directory where you keep your BIOS files (`exec.bin` and `grom.bin`)
- Typically: `RetroArch/system/freeintv_overlays/`

**Overlay Image Specifications:**
- **Dimensions**: 370×600 pixels
- **Format**: PNG image (any color depth)
- **Naming**: Match your ROM filename exactly (case-insensitive on most systems)
  - Example: If your ROM is `astrosmash.bin`, name the overlay `astrosmash.png`
  - Example: If your ROM is `nightstalker.rom`, name the overlay `nightstalker.png`

**Example Folder Structure:**
```
system/
├── exec.bin
├── grom.bin
└── freeintv_overlays/
    ├── astrosmash.png
    ├── nightstalker.png
    ├── weekendwar.png
    └── (additional game overlays...)
```

**Usage:**
1. When a game launches, FreeIntv automatically looks for a matching overlay image in the `freeintv_overlays` folder
2. If found, the overlay is displayed on the keypad area (right side of screen, or left if swapped)
3. Touch or click directly on the overlay buttons to send input to the game

#### Touch and Mouse Input
- On Android, touch the onscreen keypad overlay buttons to send input
- On Windows and Linux, use the mouse to click on the overlay buttons. Mouse clicks are mapped to input events, enabling full overlay functionality.

## Controls

### Traditional Gamepad Controls
* **Controller Swap** - Some Intellivision games expect the left controller to be player one, others expect the right controller. This isn't a problem if you have two controllers (and don't mind juggling them) but users with only one controller or using a portable setup would be effectively locked out of some games. Controller Swap swaps the two controller interfaces so that the player does not have to physically swap controllers.

### Onscreen Interactive Overlay Controls *(New)*
* **Onscreen Keypad** - Touch (Android) or click (Windows/Linux) any of the 12 buttons (1-9, *, 0, #) directly on the overlay to input commands
* **Custom Overlays** - ROM-specific overlay images display game-specific button layouts and instructions
* **Visual Feedback** - Button presses show highlighting on keypad buttons

### RetroPad Mapping

| RetroPad | FreeIntv Function |
| --- | --- |
| D-Pad| 8-way movement |
| Left Analog Stick | 16-way disc |
| Right Analog Stick | 8-way keypad |
| L3 | Keypad 0 |
| R3 | Keypad 5 |
| L2 | Keypad Clear |
| R2 | Keypad Enter |
| A | Left Action Button |
| B | Right Action Button |
| Y | Top Action Button |
| X | Use the Last Selected Intellivision Keypad Button. In Astrosmash, for example, you can leave "3" selected to enable instant access to hyperspace. |
| L/R | Activate the Mini-Keypad |
| Start | Pause Game |
| Select | Controller Swap |
