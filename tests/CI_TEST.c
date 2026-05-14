#include <slp_png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// test images name:
// rover
// gray16
// grayscale_4bit
// 4bit3
// palette_4bit
// 10.4-MB

const char path[] = "tests/test_images/10.4-MB.png";
const char new_path[] = "CI_TEST.png";


int thread_safety_test(void);









int main(void) {
    slp_image a = slp_png_read(path);
    if (a.buffer == NULL) {printf("\nread failed: %d\n", a.bit_depth);return 1;}




    // ADD FUNCTION FOR TEST HERE //
    thread_safety_test();




    int ret = slp_png_write(a, new_path);
    if (ret != 0) {printf("\nwrite failed: %d\n", ret);return 1;}
    free(a.buffer);
    // validate new saved image
    slp_image b = slp_png_read(new_path);
    if (b.buffer == NULL) {printf("\nread newly saved .png failed: %d\n", a.bit_depth);return 1;}
    free(b.buffer);
    return 0;
}









struct thread_safety_test_arg {
    const char* in_path;
    const char* out_path;
    bool* status;
};


void* thread_safety_test_task(void* arg) {

    enum {spam = 1};

    for (uint16_t i = 0; i < spam; i++) {

        struct thread_safety_test_arg data = *(struct thread_safety_test_arg*)arg;
        slp_image a = slp_png_read(data.in_path);
        if (a.buffer == NULL) {
            *data.status = false;
            return NULL;
        }



        // put more function here for thread safety check





        int ret = slp_png_write(a, data.out_path);
        if (ret != 0) {
            *data.status = false;
            return NULL;
        }
        free(a.buffer);

    }

    return NULL;
}


int thread_safety_test(void) {
    const char out_paths_prefix[] = "CI_TEST-%02hu.png";

    enum {thread_count = 50};
    pthread_t threads[thread_count] = {0};
    struct thread_safety_test_arg thread_arg[thread_count] = {0};
    char out_paths_ptr[thread_count][256] = {0};
    bool thread_status[thread_count] = {0};

    for (uint16_t i = 0; i < thread_count; i++) thread_status[i] = true;

    for (uint16_t i = 0; i < thread_count; i++) {
        snprintf(out_paths_ptr[i], 256, out_paths_prefix, i);

        thread_arg[i].in_path = path;
        thread_arg[i].out_path = out_paths_ptr[i];
        thread_arg[i].status = thread_status + i;

        if (pthread_create(threads + i, NULL, thread_safety_test_task, thread_arg + i) != 0) {
            for (int j = 0; j <= i; j++) if (pthread_join(threads[j], NULL) != 0) abort();
            return -1;
        }
    }
    for (uint16_t i = 0; i < thread_count; i++) if (pthread_join(threads[i], NULL) != 0) abort();
    for (uint16_t i = 0; i < thread_count; i++) if (!thread_arg[i].status) return 1;

    return 0;
}
