cmake_minimum_required(VERSION 3.16)

project(freeintv C)

option(INTELLIEMU_STANDALONE "Build the SDL3 standalone frontend" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(LIBRETRO_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/deps/libretro-common)

add_library(freeintv_libretro SHARED
    src/libretro.c
    src/intv.c
    src/memory.c
    src/ecs.c
    src/cp1610.c
    src/cart.c
    src/controller.c
    src/osd.c
    src/ivoice.c
    src/psg.c
    src/stic.c
    src/stb_image_impl.c
    ${LIBRETRO_COMMON_DIR}/file/file_path.c
    ${LIBRETRO_COMMON_DIR}/file/file_path_io.c
    ${LIBRETRO_COMMON_DIR}/compat/compat_posix_string.c
    ${LIBRETRO_COMMON_DIR}/compat/compat_snprintf.c
    ${LIBRETRO_COMMON_DIR}/compat/compat_strl.c
    ${LIBRETRO_COMMON_DIR}/compat/compat_strcasestr.c
    ${LIBRETRO_COMMON_DIR}/compat/fopen_utf8.c
    ${LIBRETRO_COMMON_DIR}/encodings/encoding_utf.c
    ${LIBRETRO_COMMON_DIR}/string/stdstring.c
    ${LIBRETRO_COMMON_DIR}/streams/file_stream.c
    ${LIBRETRO_COMMON_DIR}/time/rtime.c
    ${LIBRETRO_COMMON_DIR}/vfs/vfs_implementation.c
)

set_target_properties(freeintv_libretro PROPERTIES
    PREFIX ""
    C_STANDARD 17
    C_EXTENSIONS ON
)

target_include_directories(freeintv_libretro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBRETRO_COMMON_DIR}/include
)

target_compile_definitions(freeintv_libretro PRIVATE __LIBRETRO__)

target_compile_options(freeintv_libretro PRIVATE
    -Wno-sign-compare -Wunused -Wpointer-arith -Wbad-function-cast
    -Wcast-align -Waggregate-return -Wshadow -Wstrict-prototypes
    -Wformat -Wformat-security -Wwrite-strings -Wdisabled-optimization
    -fstrict-aliasing
    $<$<CONFIG:Debug>:-Wno-unused>
    $<$<CONFIG:Release>:-O2>
)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RESULT
)
if(GIT_RESULT EQUAL 0)
    target_compile_definitions(freeintv_libretro PRIVATE "GIT_VERSION=\" ${GIT_HASH}\"")
endif()

target_link_options(freeintv_libretro PRIVATE
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/link.T
    -Wl,--no-undefined
)
target_link_libraries(freeintv_libretro PRIVATE m)

if(INTELLIEMU_STANDALONE)
    find_package(SDL3 CONFIG REQUIRED)

    add_executable(intelliemu
        standalone/main.c
        standalone/config.c
        standalone/bios.c
        standalone/core.c
        standalone/input.c
        standalone/recent.c
        standalone/menu.c
    )

    set_target_properties(intelliemu PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
    )

    target_include_directories(intelliemu PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/standalone
        ${LIBRETRO_COMMON_DIR}/include
    )

    target_compile_options(intelliemu PRIVATE -Wall -Wextra)
    target_link_libraries(intelliemu PRIVATE SDL3::SDL3)
    target_compile_definitions(intelliemu PRIVATE
        "DEFAULT_CORE=\"$<TARGET_FILE:freeintv_libretro>\""
    )

    add_custom_command(TARGET intelliemu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/gamecontrollerdb.txt
            $<TARGET_FILE_DIR:intelliemu>/gamecontrollerdb.txt
    )
endif()
