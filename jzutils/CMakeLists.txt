cmake_minimum_required(VERSION 3.16)
project(jzutils LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_definitions(JZINTV_VERSION_MAJOR=0x01 JZINTV_VERSION_MINOR=0x00)
include_directories(src)

# Support code shared by the ROM/image tools (icart, bincfg, metadata, LZO, CRC, file I/O).
add_library(jz_common STATIC
    src/icart/icartbin.c
    src/icart/icartfile.c
    src/icart/icartrom.c
    src/icart/icarttag.c
    src/icart/icarttag_printer.c
    src/icart/icarttag_authors.c
    src/misc/avl.c
    src/misc/crc16.c
    src/misc/crc32.c
    src/misc/file_crc32.c
    src/misc/ll.c
    src/misc/printer.c
    src/misc/types.c
    src/plat/gnu_getopt.c
    src/plat/plat_gen.c
    src/bincfg/bincfg.c
    src/bincfg/bincfg_grmr.tab.c
    src/bincfg/bincfg_lex.c
    src/file/file.c
    src/lzoe/lzoe.c
    src/minilzo/minilzo.c
    src/metadata/metadata.c
    src/metadata/cfgvar_metadata.c
    src/metadata/icarttag_metadata.c
)
target_link_libraries(jz_common PUBLIC m)

# CP-1600 / Luigi (LUIGI) format support, used by intvname.
add_library(jz_locutus STATIC
    src/locutus/bin_to_loc.cpp
    src/locutus/loc_to_bin.cpp
    src/locutus/locutus.cpp
    src/locutus/locutus_adapt.cpp
    src/locutus/locutus_types.cpp
    src/locutus/luigi.cpp
    src/locutus/rom_to_loc.cpp
)
target_link_libraries(jz_locutus PUBLIC jz_common)

function(jz_tool name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE jz_common)
endfunction()

# Assembler and disassemblers
jz_tool(as1600
    src/asm/as1600.tab.c
    src/asm/collect.c
    src/asm/cpp_main.cpp
    src/asm/fraosub.c
    src/asm/frapsub.c
    src/asm/frasmain.c
    src/asm/fryylex.c
    src/asm/intermed.c
    src/asm/intvec.c
    src/asm/memo_string.c
    src/imasm/c_wrap.cpp
    src/imasm/parser.cpp
    src/imasm/strfifo.cpp
    src/imasm/token.cpp
    src/svn_revision.c
)
jz_tool(dasm1600 src/dasm/dasm1600.c)
jz_tool(dis1600
    src/dasm/dis1600.c
    src/dasm/exec_interp.c
    src/util/symtab.c
)
jz_tool(dasm0256
    src/util/dasm0256.c
    src/util/bitmem.c
    src/util/symtab.c
)

# ROM / image conversion and inspection
jz_tool(bin2rom src/util/bin2rom.c)
jz_tool(rom2bin src/util/rom2bin.c)
jz_tool(rom_merge src/util/rom_merge.c)
jz_tool(split_rom src/util/split_rom.c)
jz_tool(rom_metadata src/util/rom_metadata.c)
jz_tool(show_grom src/util/show_grom.c)
jz_tool(crc32 src/util/crc32.c)
jz_tool(intvname
    src/util/intvname.c
    src/name/name.c
    src/name/name_list.c
)
target_link_libraries(intvname PRIVATE jz_locutus)
