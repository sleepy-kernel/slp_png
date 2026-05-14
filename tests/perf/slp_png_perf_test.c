#include <slp_png.h>

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

// test images name:
// rover
// gray16
// grayscale_4bit
// 4bit3
// palette_4bit
// 10.4-MB

const char path[] = "tests/test_images/10.4-MB.png";
const char path_out[] = "tests/test_images/new.png";


int main(void) {
    printf("\n\n");
    clock_t start, end;
    double s[2] = {0};


    start = clock();slp_image a = slp_png_read(path);end = clock();

    if (a.buffer == NULL) {printf("read failed: %d\n", a.bit_depth);return 1;}

    s[0] = (double)(end - start) / CLOCKS_PER_SEC;
    printf("read time: %.6fs\n", s[0]);



    start = clock();int ret = slp_png_write(a, path_out);end = clock();
    if (ret != 0) {printf("save failed: %d\n", ret);return 1;}

    s[1] = (double)(end - start) / CLOCKS_PER_SEC;
    printf("write time: %.6fs\n", s[1]);



    printf("total time: %.6fs\n", s[0] + s[1]);

    free(a.buffer);
    return 0;
}
