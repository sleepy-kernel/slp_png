cmake_minimum_required(VERSION 3.20)

project("slp_png" C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library("slp_png"
    src/slp_png_read/slp_png_read_from_file.c
    src/slp_png_write/slp_png_write_to_file.c
    src/slp_image_transform/slp_image_transform.c
)

target_include_directories("slp_png" PUBLIC include)

#target_compile_options("slp_png" PUBLIC -march=native)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options("slp_png" PUBLIC -mavx2 -msse2 -Wall -Wextra -Werror -pedantic -Wformat)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options("slp_png" PUBLIC -mavx2 -msse2)
elseif(MSVC)
    target_compile_options("slp_png" PUBLIC /arch:AVX2 /arch:SSE2)
endif()

find_package(zlib-ng CONFIG REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries("slp_png"
    PUBLIC zlib-ng::zlib
    PUBLIC Threads::Threads
)

add_executable("perf_test" tests/perf_test.c)
target_link_libraries("perf_test" slp_png)

add_executable("test" tests/test_slp_png_read_write.c)
target_link_libraries("test" slp_png)
