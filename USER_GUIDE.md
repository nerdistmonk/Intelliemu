# Intelliemu User Guide

## Overview
Intelliemu runs Mattel Intellivision ROMs in a standalone window using the freeintv libretro core. It does not need RetroArch. Run `intelliemu --help` for the command-line options.

## Requirements
- Linux with the SDL3 runtime
- `release/intelliemu` and `release/freeintv_libretro.so` (see [Building](README.md#building))
- Intellivision BIOS files `exec.bin` and `grom.bin` (see [BIOS Files](#bios-files))
- Intellivision ROMs in `.int` format

---

## Running a Game

- Start a game from the command line: `./release/intelliemu Game.int`
- Or start with no game, then choose **File > Open ROM...** (Ctrl+O)
- **File** lists your 8 most recent ROMs. Select one to reopen it.
- Command-line options apply to one run and are never saved:
  - `-s DIR`: folder that holds `exec.bin` and `grom.bin`
  - `-x N`: window scale, 1-16
  - `-c PATH`: libretro core to load (default: the `freeintv_libretro.so` built with the emulator)
  - `-h`: show help

---

## BIOS Files

The emulator reads `exec.bin` and `grom.bin` from the folder in `system_dir` (default: the current directory). `-s DIR` overrides it for one run. The filenames are case-sensitive. The required MD5 hashes are listed in the [README](README.md#bios).

```
bios/
├── exec.bin
└── grom.bin
```

---

## Menus

| Menu | Items |
| --- | --- |
| File | Open ROM..., Close ROM, recent ROMs, Exit |
| Emulation | Reset, Pause / Resume |
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
| Esc | Close the menu, or quit when no menu is open |

## Controls

Keyboard, player 1:

| Key | Intellivision |
| --- | --- |
| Arrow keys | Disc |
| Z / X / C / V | Left action / right action / top action / last selected keypad button |
| Q / E | Show keypad (L / R) |
| Return | Console pause (Start) |
| Backspace | Swap left and right controllers (Select) |
| 0-9, `[`, `]` | Keypad |

Gamepad (SDL mapping, not yet checked with hardware):

| Gamepad | Intellivision |
| --- | --- |
| D-pad | Disc |
| Bottom / right / top / left face buttons | Right action / left action / last selected keypad button / top action |
| Left / right shoulder | Show keypad (L / R) |
| Start / Back | Console pause / Swap controllers |
| Left / right stick click | Keypad 0 / Keypad 5 |

## Save States

F1 saves and F5 loads the current slot (1-9). States are written next to the ROM as `<ROM name without extension>.state<N>`. For example, `Game.int` saves to `Game.state1`.

## Settings and Files

Settings live in `$XDG_CONFIG_HOME/Intelliemu/` (default: `~/.config/Intelliemu/`).

- `config.ini` is created on first run. Keys:

| Key | Values | Notes |
| --- | --- | --- |
| `scale` | 1-16 | Initial window scale. `-x` does not change it. |
| `system_dir` | folder path | Folder with `exec.bin` and `grom.bin` |
| `fullscreen` | `1` or `0` | Changed with F11 or the Video menu. Saved immediately. |
| `muted` | `1` or `0` | Changed with the Audio menu. Saved immediately. |
| `state_slot` | 1-9 | Changed with F9 or the State menu. Saved immediately. |

- `recent.txt` lists recent ROM paths, newest first.

The emulator rewrites `config.ini` whenever it saves a setting, so hand-added keys and comments are removed at that point. Invalid values are ignored and the default is used.

## Known Limitations

- **Keypad overlays are not available.** The core's overlay needs its core option and mouse or touch input. The standalone emulator does not yet provide core options or mouse input.
- **ECS is not supported.**
- **Melody Blaster and Scooby Doo** report bad opcodes and do not run yet.
- **Analog sticks, triggers, and keypad input from the analog stick** are not mapped in the standalone emulator.
- **Gamepad mapping** has not been checked with hardware.
- **Open ROM** uses the desktop's file dialog, which has not been tested on every desktop environment.

