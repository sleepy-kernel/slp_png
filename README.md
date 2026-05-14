## This is meant to be an open-source, lightweight, modern, fast PNG codec

- AVX2, SSE2 support
- Use zlib-ng
- Compatible
    - Meant to be high such that it can run on a microcontroller (still in progress)
        - As I cannot test with all hardware so, I'd need help in testing
- Efficient
    - In the current version, as tested on my machine, deflate/inflate runtime is more than 80% of the total runtime even at compression level 1, which means the runtime is mainly just zlib-ng runtime


## include && src && dependencies
- slp_png: read/write support
    - include:
        - include/slp_image.h
        - include/slp_png_read.h
        - include/slp_png_write.h
    - src:
        - src/slp_png_read/*
        - src/slp_png_write/*
    - dependencies:
        - zlib-ng

- slp_png_read: read only
    - include:
        - include/slp_image.h
        - include/slp_png_read.h
    - src:
        - src/slp_png_read/*
    - dependencies:
        - zlib-ng

- slp_png_write: write only
    - include:
        - include/slp_image.h
        - include/slp_png_write.h
    - src:
        - src/slp_png_write/*
    - dependencies:
        - zlib-ng

- slp_image_transform: image transformation tools
    - include:
        - include/slp_image.h
        - include/slp_image_transform.h
    - src:
        - src/slp_image_transform/*
    - dependencies:
        - pthreads



## Basic usage
```C
#include <slp_png.h>
#include <stdio.h>

int main()
{
    slp_image your_image = slp_png_read("/path/to/your/image");
    if (your_image.buffer == NULL) return 1;

    int ret = slp_png_write(your_image, "/path/to/where/to/write");
    if (ret != 0) return 1;

    free(your_image.buffer);

    return 0;
}
```
- NOTICE: if slp_png_read fail, your_image.bit_depth will be overwitten with a specified error code ! 
    - See in include/slp_image.h for more details about the error code


## Contribute

- Please don't vibe coding, you can't enjoy the process if AI did it for you :)
- I appreciate all your help
- I will definitely respond to your pull request
- Just freely share your idea and you don't have to be formal
- Beginners are welcome


## Support
- For slp_png_read:
    - CHUNKS:
        - For color type 0/2/4/6: IHDR, IDAT, IEND
        - For color type 3: IHDR, PLTE, tRNS, IDAT, IEND
    - Color type: 0/2/3/4/6 ( notice that color type 3 will be force convert into color type 6 )
    - Bit depth: 1/2/4/8/16 ( notice that 16 bit depth format output will stay at big-edian )
    - Compression method: 0
    - Filter method: 0
    - Interlace method: 0
    
- For slp_png_write:
    - CHUNKS: IHDR, IDAT, IEND
    - Color type: 0/2/4/6
    - Bit depth: 1/2/4/8/16 ( notice that 16 bit depth format input must be big-edian )
    - Compression method: 0
    - Filter method: 0
    - Interlace method: 0
    - Deflate compression level: 6


## Performance
- OS: Archlinux
- CPU: intel i5 12450H
- RAM: 16GB DDR5

- Test code located at: tests/perf/

- Compare with libspng:x64-linux@0.7.4

- Commands:
```bash
#! /bin/bash

git clone https://github.com/slp-c/slp_png.git

cd slp_png

cmake -S . -B build -G Ninja \
    -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_STANDARD=17 \
    -DCMAKE_C_COMPILER=/usr/bin/gcc \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" && \

cmake --build build && \

./build/slp_png_perf_test && \
./build/spng_perf_test
```


- Read time:
    - libspng: 0.136755s
    - slp_png: 0.099330s

- Write time:
    - libspng: 3.049917s
    - slp_png: 0.717744s

- Output file size:
    - libspng: 10.4 MiB
    - slp_png: 10.7 MiB

- peak RAM usage:
    - libspng: 33 MiB
    - slp_png: 33 MiB

- Notice that this test ran on a specific setup as listed above.

## Q&A
- slp PNG encoder use heuristic filtering, scoring all 5 filters (none, sub, up, avg, paeth) per scanline
    - Heuristic filtering is extremely cheap, if good filter is generated, deflate runtime will reduce significantly

- slp PNG decoder use a fixed size buffer to read IDAT chunks
    - No spikes in RAM usage for large IDAT
    - size = 65536, allocated on the stack

- Full CRC32 validation for all supported chunks
