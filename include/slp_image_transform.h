#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <slp_png.h>


bool slp_image_convert_to_8bit(struct slp_image *image);
bool slp_image_convert_to_16bit(struct slp_image *image);

bool slp_image_crop(struct slp_image *image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height);
bool slp_image_linear_transform(struct slp_image *image, const double* A, const uint8_t* background); // A[4] = a00, a01, a10, a11

bool slp_image_convert_G8_to_RGBA32(struct slp_image *image);
bool slp_image_convert_GA16_to_RGBA32(struct slp_image *image);

bool slp_image_convert_G16_to_RGBA64(struct slp_image *image);
bool slp_image_convert_GA32_to_RGBA64(struct slp_image *image);

struct slp_image slp_image_copy(const struct slp_image image);

bool slp_image_unformat(struct slp_image *image);
bool slp_image_format(struct slp_image *image);
