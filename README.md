## This is meant to be an open-source, lightweight, modern, fast PNG codec

- AVX2, SSE2 support
- Use zlib-ng
- Compatible
    - Meant to be high such that it can run on a microcontroller (still in progress)
        - As I cannot test with all hardware so, I'd need help in testing
- Efficient
    - In the current version, as tested on my machine, deflate/inflate runtime is more than 80% of the total runtime even at compression level 1, which means the runtime is mainly just zlib-ng runtime
- **NOTICE**: All this is possible because of how well-defined PNG format is


## Project structure
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
    - Color type: 0/2/3/4/6
        - NOTICE that color type 3 will be force convert into RGBA32 ( color type 6, bit depth 8 )
    - Bit depth: 1/2/4/8/16
        - NOTICE that for bit depth 16 format, output is always big-edian
    - Compression method: 0
    - Filter method: 0
    - Interlace method: 0
    - Full CRC32 validation for all supported chunks
    - Use fixed size buffer for IDAT chunks decode
        - No RAM spikes when decode PNG with big IDAT
        - Buffer size = 65536 bytes

- For slp_png_write:
    - CHUNKS: IHDR, IDAT, IEND
    - Color type: 0/2/4/6
    - Bit depth: 1/2/4/8/16
        - NOTICE that for bit depth 16 format, input must be big-edian
    - Compression method: 0
    - Filter method: 0
    - Interlace method: 0
    - Compression level: 6
    - Heuristic filtering with all 5 filter type
        - Heuristic filtering is extremely cheap, if good filter is generated, deflate runtime will reduce significantly

- For both slp_png_read and slp_png_write:
    - Thread-safe: this function can call by any thread, but it does not automatically handle fileIO conflicts
    - Allocation: mainly malloc, stack allocation via arrays are small
        - Specifically, total size of all array allocated on the stack is only 57 bytes
        - Low risk of stack overflow


## Performance
- OS: Archlinux
- CPU: intel i5 12450H
- RAM: 16GB DDR5

- Test code located at: tests/perf/

- Compare with libspng:x64-linux@0.7.4

- Commands:
```bash
#! /bin/bash
set -euo pipefail

git clone https://github.com/slp-c/slp_png.git
cd slp_png

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

cmake --build build

./build/slp_png_perf_test
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


## Quick test on minimal setup
- Platform: Linux
- Pakages: glibc gcc zlib-ng
- Optional pakages: valgrind

```bash
#! /bin/bash
set -euo pipefail
shopt -s nullglob

# clone the repo
git clone https://github.com/slp-c/slp_png.git
cd slp_png
project_root=$PWD

# manual build
mkdir build
cd build
gcc -c $project_root/src/*/*.c \
    -I $project_root/include \
    -march=native -mtune=native -O3

ar rcs libslp_png.a *.o
rm *.o

gcc -c $project_root/tests/perf/*.c $project_root/tests/*.c \
    -I $project_root/include \
    -march=native -mtune=native -O3

# run executables
for file in $project_root/build/*.o; do
    executable=${file%.o}

    cd $project_root/build
    gcc $file \
        -L $project_root/build \
        -o $executable \
        -lz-ng -lspng -pthread -Wl,-Bstatic -lslp_png -Wl,-Bdynamic
    
    cd $project_root
#uncomment if you want to test with valgrind
#valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes --error-exitcode=1 $executable valgrind > /dev/null
    $executable
    rm $executable
    rm $file
done

# test output
set +e
cd $project_root
for file in $project_root/CI_TEST-*.png; do
    cmp $file $project_root/CI_TEST.png
    if [ $? -ne 0 ]; then
        echo "fail at: cmp $file $project_root/CI_TEST.png"
        exit 1
    fi
done
set -e

# clean up
echo "
test success"
cd $project_root/..
rm -rf $project_root

shopt -u nullglob
```
