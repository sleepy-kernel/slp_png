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

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options("slp_png" PUBLIC -mavx2 -msse2 -Wall -Wextra -Werror -pedantic -Wformat -Wno-c++98-compat)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options("slp_png" PUBLIC -mavx2 -msse2)
    elseif(MSVC)
        target_compile_options("slp_png" PUBLIC /arch:AVX2 /arch:SSE2)
    endif()
endif()



if(WIN32)
    find_package(zlib-ng CONFIG REQUIRED)
    find_package(PThreads4W REQUIRED)
    target_link_libraries("slp_png"
        PUBLIC zlib-ng::zlib
        PUBLIC PThreads4W::PThreads4W
    )
else()
    find_package(zlib-ng CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries("slp_png"
        PUBLIC zlib-ng::zlib
        PUBLIC Threads::Threads
    )
endif()



# tests executable
find_package(SPNG CONFIG REQUIRED)

add_executable("slp_png_perf_test" tests/perf/slp_png_perf_test.c)
add_executable("spng_perf_test"    tests/perf/spng_perf_test.c)

target_link_libraries("slp_png_perf_test" PRIVATE slp_png)
target_link_libraries("spng_perf_test"    PRIVATE $<IF:$<TARGET_EXISTS:spng::spng>,spng::spng,spng::spng_static>)

if (!MSVC)
    target_compile_options("slp_png_perf_test" PUBLIC -march=native -mtune=native -O3)
    target_compile_options("spng_perf_test"    PUBLIC -march=native -mtune=native -O3)
endif()


add_executable("test" tests/CI_TEST.c)
if(WIN32)
    target_link_libraries("test"
        PRIVATE slp_png
        PRIVATE PThreads4W::PThreads4W
    )
else()
    target_link_libraries("test"
        PRIVATE slp_png
        PRIVATE Threads::Threads
    )
endif()
