# Intelliemu User Guide

## Overview
Intelliemu runs Mattel Intellivision ROMs in a standalone window using the freeintv libretro core. It does not need RetroArch. Run `intelliemu --help` for the command-line options.

## Requirements
- Linux with the SDL3 runtime
- `release/intelliemu` and `release/freeintv_libretro.so` (see [Building](README.md#building))
- Intellivision BIOS files (see [BIOS Files](#bios-files))
- Intellivision ROMs in `.int` format

---

## Running a Game

- Start a game from the command line: `./release/intelliemu Game.int`
- Or start with no game, then choose **File > Open ROM...** (Ctrl+O)
- **File** lists your 8 most recent ROMs. Select one to reopen it.
- Command-line options apply to one run and are never saved:
  - `-s DIR`: extra folder searched for BIOS files
  - `-w N`: window scale, 1-16
  - `-f`: start in fullscreen
  - `-x`, `--no-xbrz`: start with the xBRZ filter on or off
  - `-d N`: exit after N frames (requires a game; for testing)
  - `-c PATH`: libretro core to load (default: the `freeintv_libretro.so` built with the emulator)
  - `-h`: show help

---

## BIOS Files

The emulator searches these folders, in order, for each BIOS image (`-s` only adds the last one, for one run):

1. `~/.config/Intelliemu/bios/` (or `$XDG_CONFIG_HOME/Intelliemu/bios/`)
2. `bios/` beside `release/intelliemu`
3. The folder in `system_dir` (default: the current directory)

Accepted filenames are the same as in the [README](README.md#bios). Names match regardless of case, punctuation, a leading `INTV`, and a `.bin` or `.rom` extension, so `INTV_EROM.bin` and `intv-erom.BIN` both work. A file must be exactly 8192 bytes (executive) or 2048 bytes (graphics); other sizes are skipped with a message. If an image is missing, the emulator prints the folders it searched.

`ECS.bin` is optional and must be exactly 24576 bytes. Put it in the same folders. Only the ECS cartridges need it; the emulator prints a note at startup when it is absent, and a warning when an ECS cartridge is loaded without it.

```
~/.config/Intelliemu/bios/
├── INTV_EROM.bin
└── INTV_GROM.bin
```

---

## Menus

| Menu | Items |
| --- | --- |
| File | Open ROM..., Close ROM, recent ROMs, About IntelliEmu..., Exit |
| Emulation | Reset, Pause / Resume, ECS keyboard, Remap Right keyboard..., Remap Left keyboard..., Remap Right gamepad..., Remap Left gamepad..., Swap Right and Left gamepads, Restore default bindings |
| State | Save slot N, Load slot N, Next slot |
| Video | Fullscreen, 4:3 aspect, xBRZ filter, Screenshot (PNG) |
| Audio | Mute, sample rate (11025, 22050, 44100, 48000 Hz), Volume 100% or 50% |

## Hotkeys

| Key | Action |
| --- | --- |
| Ctrl+O | Open ROM |
| Ctrl+W | Close ROM |
| Ctrl+Q | Quit |
| F1 / F5 | Save / load state in the current slot |
| F9 | Next state slot (1-9) |
| F10 | Reset |
| F11 | Toggle fullscreen (the menu bar is hidden in fullscreen and any open menu closes; leave fullscreen with F11 to get it back) |
| F12 | Save a PNG screenshot of the game (no menu or xBRZ filter) to the current folder as `screenshot_NNN.png` |
| Esc | Close the menu or remap screen, or quit when neither is open |

## Controls

Keyboard defaults, Right port:

| Key | Intellivision |
| --- | --- |
| W / A / S / D | Disc up / left / down / right |
| Q / E | Left action / right action |
| Tab / R | Top action / bottom action |
| Z / X | Start / Select |
| Return / Backspace | L / R button |
| 1-9, 0 | Keypad 1-9, Keypad 0 |
| C / V | Keypad Clear / Keypad Enter |

Keyboard defaults, Left port:

| Key | Intellivision |
| --- | --- |
| Arrow keys | Disc |
| Delete / Page Down | Left action / right action |
| Home / End | Top action / bottom action |
| Numpad / / Numpad * | Start / Select |
| Insert / Page Up | L / R button |
| Numpad 1-9, Numpad 0 | Keypad 1-9, Keypad 0 |
| Numpad . / Numpad Enter | Keypad Clear / Keypad Enter |

Each port has its own bindings. A control responds to its keyboard key or its gamepad input, whichever is pressed. Within one port, each key belongs to one control, and each gamepad input belongs to one control. Binding a key or input that is already in use swaps the two controls.

Each **Remap** entry in the Emulation menu edits one port's bindings for one device: **Remap Right keyboard...**, **Remap Left keyboard...**, **Remap Right gamepad...**, or **Remap Left gamepad...**. On a remap screen:

- Arrow keys move the selection. The Left and Right arrow keys switch between the Controller and Keypad columns. Return starts a rebind. On keyboard screens, press the new key to finish. On gamepad screens, press the new gamepad button or trigger to finish.
- Delete leaves the selected control unbound. An unbound control does nothing from that device.
- If the new key or gamepad input is already used by another control of that port, the two bindings swap.
- Escape cancels a rebind, or closes the remap screen when no rebind is in progress.
- While the remap screen is open, the game receives no input.
- Keyboard screens: Escape, F1, F5, F9, F10, F11, F12, and Ctrl+O/W/Q are reserved for the emulator and cannot be bound.
- Gamepad screens are navigated with the keyboard. Gamepad input is captured only during a rebind, and buttons that SDL does not name cannot be bound.

**Emulation > Restore default bindings** resets both ports' keyboard bindings to the defaults and clears every gamepad binding.

**Emulation > ECS keyboard** (off by default) turns the keyboard into the ECS keyboard. While it is on, the keyboard no longer drives Intellivision controls, including the disc and keypad keys, and the two keyboard remap entries are disabled. Gamepads still work, and the emulator hotkeys (F1, F5, F9 to F12, Esc, Ctrl+Q) still work. The setting is saved as `ecs_keyboard` in `config.ini`.

With the ECS keyboard on, each key sends the character it types, with no remapping. Letters, digits, and the punctuation on the ECS keyboard work. Holding Shift sends the ECS Shift layer, so Shift+. produces `>`. Return is ENTER, Space is Space, the arrow keys are the ECS arrows, Ctrl is the ECS CTL key, and Backspace is the ECS ESC key. Keys with no ECS equivalent, such as `!`, `@`, `[`, and Tab, do nothing. Esc is still the emulator's menu key, and F1, F5, and F9 to F12 are emulator hotkeys. The ECS keyboard has no function keys, so nothing is lost there.

Gamepad bindings start empty. Bind any gamepad button or trigger to any control with **Remap Right gamepad...** or **Remap Left gamepad...**. A connected gamepad goes to the first free port, Right before Left. Gamepads are told apart by serial number when they report one. Identical gamepads without a serial number are told apart by connection order.

When a gamepad connects, the window shows a notice for a few seconds naming its port and controller, such as `Right gamepad: Xbox 360 Controller`. If both ports already have a gamepad, the notice says `Gamepad not assigned: both ports already have one`. The bottom line of each remap screen lists the gamepad on each port, or `none`.

**Emulation > Swap Right and Left gamepads** exchanges the two ports' gamepads. Bindings stay with their ports. The swap is saved: each swapped gamepad returns to its new port when it reconnects, if that port is free. The notice lists the gamepad on each port, or says `No gamepad connected to swap` when neither port has one.

Controller layouts come from `gamecontrollerdb.txt`, which sits beside the `intelliemu` executable. If the file is missing, the emulator prints a warning and uses only SDL's built-in mappings.

When several keypad buttons are held, the most recently pressed one that is still held is the one the core receives. Keypad buttons combine with the other controls, so a disc direction can be held while a keypad button is pressed.

## Save States

F1 saves and F5 loads the current slot (1-9). States are written next to the ROM as `<ROM name without extension>.state<N>`. For example, `Game.int` saves to `Game.state1`.

## Settings and Files

Settings live in `$XDG_CONFIG_HOME/Intelliemu/` (default: `~/.config/Intelliemu/`).

- `config.ini` is created on first run. Keys:

| Key | Values | Notes |
| --- | --- | --- |
| `scale` | 1-16 | Initial window scale. `-x` does not change it. |
| `system_dir` | folder path | Extra folder searched for BIOS files |
| `open_dir` | folder path | Folder the open ROM dialog starts in. Set automatically to the folder of the last ROM opened. Saved immediately. |
| `fullscreen` | `1` or `0` | Changed with F11 or the Video menu, saved immediately, and restored at startup. |
| `aspect_4_3` | `1` or `0` | `1` shows a 4:3 picture, `0` stretches to fill the window. Both scale to fit the window. Changed with the Video menu. Default `1`. |
| `xbrz` | `1` or `0` | Smooths diagonal edges with a 2× xBRZ-style filter. Changed with the Video menu. Default `0`. |
| `muted` | `1` or `0` | Changed with the Audio menu. Saved immediately. |
| `sample_rate` | `11025`, `22050`, `44100` or `48000` | Audio output rate. Changed with the Audio menu. Default `44100`. The core's 44100 Hz audio is resampled to this rate. |
| `volume` | 0-100 | Audio volume as a percentage. Changed with the Audio menu (100 or 50). Default `100`. |
| `right_gamepad`, `left_gamepad` | Gamepad identifier, or empty for none | Written by **Swap Right and Left gamepads**. Saved immediately. A gamepad is identified by its serial number when it has one, otherwise by its GUID and a `#n` suffix. Only swapped gamepads are remembered. |
| `state_slot` | 1-9 | Changed with F9 or the State menu. Saved immediately. |
| `right_key_<control>`, `left_key_<control>` | SDL key name, such as `W`, `Up`, or `Keypad 1` | One line per keyboard control for each port, for example `right_key_disc_up`. An empty value leaves the control unbound. Unknown names and reserved keys keep the current binding. If two controls of one port share a key on load, the later one is unbound. Written by the remap screens. Keys from older versions (`p1_*`, `p2_*`, `p1_left`, `key_*`) are ignored, so bindings start from the defaults. |
| `right_pad_<control>`, `left_pad_<control>` | SDL gamepad button name, such as `a`, `leftshoulder`, or `lefttrigger` | One line per gamepad control for each port, for example `right_pad_top_action`. An empty value leaves the control unbound. Unknown names keep the current binding. Written by the remap screens. |

- `recent.txt` lists recent ROM paths, newest first.

The emulator rewrites `config.ini` whenever it saves a setting, so hand-added keys and comments are removed at that point. Invalid values are ignored and the default is used.

## Known Limitations

- **Keypad overlays are not available.** The core's overlay needs its core option and mouse or touch input. The standalone emulator does not yet provide core options or mouse input.
- **The left stick is not bindable.** It drives the disc.
- **The right stick drives the keypad directions** (Tron Deadly Discs). It is not bindable. Its click and both triggers can be bound as gamepad inputs.
- **Keypad input from a stick is not supported.** Only the right stick reaches the keypad directions.
- **ECS keyboard maps by character.** The ECS keyboard matrix is emulated, and the host keyboard sends the character it types. Backspace is the ECS ESC key. Esc stays the emulator's menu key, so the ECS ESC key is only reachable through Backspace. Music keyboard games, such as Melody Blaster, need a real music keyboard and will not be playable with the host keyboard. The ECS PSG2 sound chip is emulated and mixed with PSG1.
- **ECS titles are only partly tested.** Mind Strike responds to the ECS keyboard, and Jetsons registers Backspace (ECS ESC). Number Jumble's menu reads the keypad, not the ECS keyboard. Scooby-Doo's Maze Chase works with the ECS keyboard (number row for the maze and player count, then the arrow keys). World Series boots to its title screen, but its ECS keyboard play is not fully tested, and Jetsons word entry is not yet verified.
