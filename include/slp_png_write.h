#pragma once
#include "slp_image.h"
#include <stdbool.h>

// not support interlace and color type 3 imsave

// return 3, deflate fail
// return 2, wrong input
// return  1, file write failure
// return -1, malloc fail
// return 0, success
int slp_png_write(struct slp_image image, const char* path);
