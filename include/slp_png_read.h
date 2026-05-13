#pragma once
#include "slp_image.h"
#include <stdbool.h>


struct slp_image slp_png_read(const char path[]);


// only call free(image->buffer); and set image to 0
void slp_image_delete(struct slp_image *image);
