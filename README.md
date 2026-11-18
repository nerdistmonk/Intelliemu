# Intelliemu

Intelliemu is a standalone Mattel Intellivision emulator. It runs the freeintv libretro core in an SDL3 window, without RetroArch. Linux only.

## Authors & Contributors

freeintv was created by David Richardson.
The PSG and STIC emulation was made closer to hardware and optimized by Oscar Toledo G. (nanochess), who also added save states.

The Intellivoice code has been contributed by Joe Zbiciak (author of jzintv), and adapted by Oscar Toledo G. (nanochess)

**Onscreen Overlay Enhancement**: Onscreen interactive keypad overlays with touchscreen (Android) and mouse (Windows/Linux) support (November 2025) by Jason Carr

<div style="margin: 18px 0;"><img width="1291" height="727" alt="FreeIntv Screenshot 1" src="screenshots/Screenshot 2025-11-24 194829.png" style="border:2px solid #FFD700; border-radius:6px;" /></div>
<div style="margin: 18px 0;"><img width="1372" height="770" alt="FreeIntv Screenshot 2" src="screenshots/Screenshot 2025-11-24 194901.png" style="border:2px solid #FFD700; border-radius:6px;" /></div>
<div style="margin: 18px 0;"><img width="1365" height="765" alt="FreeIntv Screenshot 3" src="screenshots/Screenshot 2025-11-24 194935.png" style="border:2px solid #FFD700; border-radius:6px;" /></div>

## Building

Requirements: Linux, CMake 3.16+, GCC, and SDL3 development files (tested with SDL 3.4.20; build SDL3 from source if your distribution does not package it).

```sh
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release   # configure
cmake --build release                             # build
```

Output goes to `release/`:
- `intelliemu`: the standalone emulator
- `freeintv_libretro.so`: the libretro core, which `intelliemu` loads at runtime

To build only the libretro core without SDL3, configure with `-DINTELLIEMU_STANDALONE=OFF`. If CMake cannot find SDL3, add `-DCMAKE_PREFIX_PATH=<prefix>` to the configure step.

## Quick Start

1. Put the two BIOS files, `exec.bin` and `grom.bin`, in one folder (see [BIOS](#bios)).
2. Start a game with that folder:

   ```sh
   ./release/intelliemu -s /path/to/bios Game.int
   ```

   Or run `./release/intelliemu` and open a ROM with Ctrl+O.

## Documentation

See [USER_GUIDE.md](USER_GUIDE.md) for running games, menus, controls, settings, and known limitations.

## License
The FreeIntv core is licensed under GPLv2+. More information at https://github.com/libretro/FreeIntv/blob/master/LICENSE

## BIOS
Intelliemu needs these two Intellivision BIOS files in the folder given by `-s` or the `system_dir` setting. See [BIOS Files](USER_GUIDE.md#bios-files).

| Function | Filename* | MD5 Hash |
| --- | --- | --- | 
| Executive ROM | `exec.bin`  | `62e761035cb657903761800f4437b8af` |
| Graphics ROM | `grom.bin` | `0cd5946c6473e42e8e4c2137785e427f` |

* BIOS filenames are case-sensitive

## Entertainment Computer System
FreeIntv does not currently support Entertainment Computer System (ECS) functionality. Contributions to the code are welcome!
