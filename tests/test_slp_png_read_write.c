#include <slp_png.h>
#include <stdio.h>
#include <stdlib.h>

// test images name:
// rover
// gray16
// grayscale_4bit
// 4bit3
// palette_4bit
// 10.4-MB

const char path[] = "tests/test_images/10.4-MB.png";
const char new_path[] = "tests/test_slp_png_read_write.png";

int main(void) {
    slp_image a = slp_png_read(path);
    if (a.buffer == NULL) {printf("\nread failed: %d\n", a.bit_depth);return 1;}




    // ADD FUNCTION FOR TEST HERE //




    int ret = slp_png_write(a, new_path);
    if (ret != 0) {printf("\nwrite failed: %d\n", ret);return 1;}
    free(a.buffer);
    return 0;
}
