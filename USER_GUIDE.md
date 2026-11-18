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
  - `-x N`: window scale, 1-16
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
| File | Open ROM..., Close ROM, recent ROMs, Exit |
| Emulation | Reset, Pause / Resume, Remap P1 keyboard..., Remap P2 keyboard..., Remap P1 gamepad..., Remap P2 gamepad..., Restore default bindings |
| State | Save slot N, Load slot N, Next slot |
| Video | Fullscreen |
| Audio | Mute |

## Hotkeys

| Key | Action |
| --- | --- |
| Ctrl+O | Open ROM |
| Ctrl+W | Close ROM |
| Ctrl+Q | Quit |
| F1 / F5 | Save / load state in the current slot |
| F9 | Next state slot (1-9) |
| F10 | Reset |
| F11 | Toggle fullscreen |
| Esc | Close the menu or remap screen, or quit when neither is open |

## Controls

Keyboard defaults, player 1:

| Key | Intellivision |
| --- | --- |
| W / A / S / D | Disc up / left / down / right |
| Q / E | Left action / right action |
| Tab / R | Top action / bottom action |
| Z / X | Start / Select |
| Return / Backspace | L / R button |
| 1-9, 0 | Keypad 1-9, Keypad 0 |
| C / V | Keypad Clear / Keypad Enter |

Keyboard defaults, player 2:

| Key | Intellivision |
| --- | --- |
| Arrow keys | Disc |
| Delete / Page Down | Left action / right action |
| Home / End | Top action / bottom action |
| Numpad / / Numpad * | Start / Select |
| Insert / Page Up | L / R button |
| Numpad 1-9, Numpad 0 | Keypad 1-9, Keypad 0 |
| Numpad . / Numpad Enter | Keypad Clear / Keypad Enter |

Each player has their own bindings. A control responds to its keyboard key or its gamepad input, whichever is pressed. Within one player, each key belongs to one control, and each gamepad input belongs to one control. Binding a key or input that is already in use swaps the two controls.

Each **Remap** entry in the Emulation menu edits one player's bindings for one device: **Remap P1 keyboard...**, **Remap P2 keyboard...**, **Remap P1 gamepad...**, or **Remap P2 gamepad...**. On a remap screen:

- Arrow keys move the selection. Left and Right switch between the Controller and Keypad columns. Return starts a rebind. On keyboard screens, press the new key to finish. On gamepad screens, press the new gamepad button or trigger to finish.
- Delete leaves the selected control unbound. An unbound control does nothing from that device.
- If the new key or gamepad input is already used by another control of that player, the two bindings swap.
- Escape cancels a rebind, or closes the remap screen when no rebind is in progress.
- While the remap screen is open, the game receives no input.
- Keyboard screens: Escape, F1, F5, F9, F10, F11, and Ctrl+O/W/Q are reserved for the emulator and cannot be bound.
- Gamepad screens are navigated with the keyboard. Gamepad input is captured only during a rebind, and buttons that SDL does not name cannot be bound.

**Emulation > Restore default bindings** resets both players' keyboard bindings to the defaults and clears every gamepad binding.

Gamepad bindings start empty. Bind any gamepad button or trigger to any control with **Remap P1 gamepad...** or **Remap P2 gamepad...**. Player 1 uses the first connected gamepad and player 2 the second, in connection order.

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
| `fullscreen` | `1` or `0` | Changed with F11 or the Video menu, saved immediately, and restored at startup. |
| `muted` | `1` or `0` | Changed with the Audio menu. Saved immediately. |
| `state_slot` | 1-9 | Changed with F9 or the State menu. Saved immediately. |
| `p1_key_<control>`, `p2_key_<control>` | SDL key name, such as `W`, `Up`, or `Keypad 1` | One line per keyboard control for each player, for example `p1_key_disc_up`. An empty value leaves the control unbound. Unknown names and reserved keys keep the current binding. If two controls of one player share a key on load, the later one is unbound. Written by the remap screens. Old `key_<control>` lines are ignored. |
| `p1_pad_<control>`, `p2_pad_<control>` | SDL gamepad button name, such as `a`, `leftshoulder`, or `lefttrigger` | One line per gamepad control for each player, for example `p1_pad_top_action`. An empty value leaves the control unbound. Unknown names keep the current binding. Written by the remap screens. |

- `recent.txt` lists recent ROM paths, newest first.

The emulator rewrites `config.ini` whenever it saves a setting, so hand-added keys and comments are removed at that point. Invalid values are ignored and the default is used.

## Known Limitations

- **Keypad overlays are not available.** The core's overlay needs its core option and mouse or touch input. The standalone emulator does not yet provide core options or mouse input.
- **The right stick is not mapped.** Its click and both triggers can be bound as gamepad inputs. The left stick drives the disc, and keypad input from a stick is not supported.
- **ECS is not supported.**
- **Melody Blaster and Scooby Doo** report bad opcodes and do not run yet.
