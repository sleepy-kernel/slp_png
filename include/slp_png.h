#pragma once
#include <stdint.h>
#include <stdbool.h>


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
    // 255 = malloc fail
    // 1 = file read failure ( file is not open, false file size )
    // 2 = invalid file format ( file does not follow the PNG specification or Interlaced 1 )
    // 3 = inflate failure ( zlib-ng inflate failure )
}slp_image;





struct slp_image slp_png_read(const char path[]);




// only call free(image->buffer); and set image to 0
void slp_image_delete(struct slp_image *image);





// not support interlace and color type 3 imsave

// return 3, deflate fail
// return 2, wrong input
// return  1, file write failure
// return -1, malloc fail
// return 0, success
int slp_png_write(struct slp_image image, const char* path);