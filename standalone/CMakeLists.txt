cmake_minimum_required(VERSION 3.16)

project(intelliemu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

find_package(SDL3 CONFIG REQUIRED)

add_executable(intelliemu
    main.c
    core.c
    input.c
    recent.c
    menu.c
)

target_include_directories(intelliemu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/deps/libretro-common/include
)

target_compile_options(intelliemu PRIVATE -Wall -Wextra)
target_link_libraries(intelliemu PRIVATE SDL3::SDL3)

if(TARGET freeintv_libretro)
    target_compile_definitions(intelliemu PRIVATE
        "DEFAULT_CORE=\"$<TARGET_FILE:freeintv_libretro>\""
    )
endif()
