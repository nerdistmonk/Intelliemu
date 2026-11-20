# jzutils

Command-line tools for Intellivision ROMs and the CP-1600 CPU, taken from
[jzIntv](http://spatula-city.org/~im14u2c/intv/) (version 20200712) with the
emulator removed. It includes the CP-1600 assembler, disassemblers, and
ROM/cart format converters.

## Tools

| Group | Tools |
|-------|-------|
| Assembler | `as1600` |
| Disassemblers | `dis1600`, `dasm1600`, `dasm0256` |
| ROM and cart formats | `rom2bin`, `bin2rom`, `rom_merge`, `rom_metadata`, `split_rom`, `show_grom`, `intvname` |
| Checksums | `crc32` |

## Build

Requires CMake and a GCC toolchain (C99 and C++14).

```sh
cmake -S jzutils -B build
cmake --build build
```

The binaries are written to `build/bin/`.

## Test

```sh
jzutils/test.sh build/bin
```

This runs a few known-answer and golden-output checks against the built tools.

## Notes

- Built without the emulator. Only the emulator-free tools from jzIntv's
  `bin/` directory are included. `jzintv` and `jzintv_batch` are not.
- `as1600` is built without an SVN revision, so its banner reads
  "revision unknown". The behaviour is otherwise unchanged.
- The `periph`, `cp1600`, and `serializer` headers are kept only because
  `intvname` uses their types through the Locutus adapter. No emulator
  runtime is built.

## License and credit

These tools are GPLv2, from jzIntv by Joe Zbiciak (`intvnut`). See
`COPYING.txt`. Sources are copied verbatim from upstream, keeping their original file
layout. Three unused upstream sources were omitted, and `CMakeLists.txt` is new.
