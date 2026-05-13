cmake_minimum_required(VERSION 3.20)

project("slp_png" C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library("slp_png"
    src/slp_png_read/slp_png_read_to_file.c
    src/slp_png_write/slp_png_write_to_file.c
    src/slp_image_transform/slp_image_transform.c
)

target_include_directories("slp_png" PUBLIC include)

target_compile_options("slp_png" PUBLIC -march=native -O3)

find_package(zlib-ng CONFIG REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries("slp_png"
    PUBLIC zlib-ng::zlib
    PUBLIC Threads::Threads
)

add_executable("test" test.c)
target_link_libraries("test" slp_png)

