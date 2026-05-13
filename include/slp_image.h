#pragma once
#include <stdint.h>


// typedef is not required, can remove safely

// the Image
// remember to check the Image state if imread have success or not
typedef struct slp_image {
    uint8_t* buffer; // data of the image, allocate via malloc
    uint32_t height;
    uint32_t width;
    uint32_t channels;
    uint8_t bit_depth;
    // this is for debuging:
    // bit depth =
    // 0 = success
    // -1 = malloc fail
    // 1 = file read failure ( file is not open, false file size )
    // 2 = invalid file format ( file does not follow the PNG specification or Interlaced 1)
    // 3 = inflate failure ( zlib-ng inflate failure )
    // 4 = thread create/join fail
}slp_image;
