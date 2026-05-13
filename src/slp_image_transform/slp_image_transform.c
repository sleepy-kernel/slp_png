/*
Copyright 2026 slp-c

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <slp_image_transform.h>
#include <slp_image.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <immintrin.h>
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
static int64_t ceil__(double x);


// only take miliseconds to run
// mem: O(1)
bool slp_image_convert_to_8bit(struct slp_image *image) {

    uint8_t* src = image->buffer;
    uint8_t* dest = image->buffer;

    const uint64_t size = image->width * image->height * image->channels; // dest size

    switch (image->bit_depth) {
        case 1: {
            uint64_t i = 0;
            #ifdef __AVX2__
            __m256i all1 = _mm256_set1_epi8(-1);
            __m256i zero = _mm256_setzero_si256();

            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)src);
                in = _mm256_blendv_epi8(zero, all1, in);
                _mm256_storeu_si256((__m256i*)dest, in);
                src+=32;dest+=32;
            }
            #endif
            for (; i < size; i++) *dest++ = *src++ * 255;
            break;
        }
        case 2: {
            uint64_t i = 0;
            #ifdef __AVX2__
            __m256i scalar = _mm256_set1_epi8(85);
            __m256i zero = _mm256_setzero_si256();

            for (; i += 32 <= size; i+= 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)src);
                __m256i in_lo = _mm256_unpacklo_epi8(in, zero);
                __m256i in_hi = _mm256_unpackhi_epi8(in, zero);
                in_lo = _mm256_mullo_epi16(in_lo, scalar);
                in_hi = _mm256_mullo_epi16(in_hi, scalar);
                in = _mm256_packus_epi16(in_lo, in_hi);
                _mm256_storeu_si256((__m256i*)dest, in);
                src+=32;dest+=32;
            }
            #endif
            for (; i < size; i++) *dest++ = *src++ * 85;
            break;
        }
        case 4: {
            uint64_t i = 0;
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)src);
                in = _mm256_or_si256(in, _mm256_slli_epi64(in, 4));
                _mm256_storeu_si256((__m256i*)dest, in);
                src+=32;dest+=32;
            }
            #endif
            for (; i < size; i++) *dest++ = *src++ * 17;
            break;
        }
        case 8: return true;
        case 16: {
            uint16_t random_value_for_edian_test = 1;
            const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

            uint64_t i = 0;
            #ifdef __AVX2__
            __m256i zero = _mm256_setzero_si256();
            for (; i + 16 <= size; i += 16) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i*2));
                in = _mm256_srli_epi16(in, 8);
                in = _mm256_packus_epi16(in, zero);
                *(uint64_t*)(dest + i + 8 * 0) = _mm256_extract_epi64(in, 0);
                *(uint64_t*)(dest + i + 8 * 1) = _mm256_extract_epi64(in, 2);
            }
            #endif
            
            for (; i < size; i++) dest[i] = src[i*2 + is_little_edian];
            break;
        }
        default: return false;
    }
    image->bit_depth = 8;
    return true;
}







// only take miliseconds to run
// mem: O(N)
// return false = malloc fail or input wrong
bool slp_image_convert_to_16bit(struct slp_image *image) {

    const uint64_t size = (uint64_t)image->height * (uint64_t)image->width * (uint64_t)image->channels; // source size

    uint8_t* new_buffer = (uint8_t*)malloc(size * 2);
    if (new_buffer == NULL) {
        if (image->bit_depth == 16) return true;
        return false;
    }

    uint8_t* src = image->buffer;
    uint16_t* dest = (uint16_t*)new_buffer;

    uint64_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            __m256i zero = _mm256_setzero_si256();
            __m256i all1 = _mm256_set1_epi16(-1);
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)src);
                __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                __m256i in_hi = _mm256_unpackhi_epi8(in, in);
                in_lo = _mm256_blendv_epi8(zero, all1, in_lo);
                in_hi = _mm256_blendv_epi8(zero, all1, in_hi);
                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(in_lo));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(in_hi));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(in_lo, 1));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(in_hi, 1));
                dest+=16;
                src+=32;
            }
            #endif
            uint16_t* d = (uint16_t*)(dest);
            for (; i < size; i++) *d++ = *src++ * 65535;
            break;
        }
        case 2: {
            #ifdef __AVX2__
            __m256i zero = _mm256_setzero_si256();
            __m256i scalar = _mm256_set1_epi16(21845);
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)src);
                __m256i in_lo = _mm256_unpacklo_epi8(in, zero);
                __m256i in_hi = _mm256_unpackhi_epi8(in, zero);
                in_lo = _mm256_mullo_epi16(in_lo, scalar);
                in_hi = _mm256_mullo_epi16(in_hi, scalar);
                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(in_lo));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(in_hi));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(in_lo, 1));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(in_hi, 1));
                dest+=16;
                src+=32;
            }
            #endif
            uint16_t* d = (uint16_t*)(dest);
            for (; i < size; i++) *d++ = *src++ * 21845;
            break;
        }
        case 4: {
            #ifdef __AVX2__
            __m256i zero = _mm256_setzero_si256();
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)src);
                in = _mm256_or_si256(in, _mm256_slli_epi64(in, 4));
                __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                __m256i in_hi = _mm256_unpackhi_epi8(in, in);
                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(in_lo));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(in_hi));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(in_lo, 1));
                dest+=16;
                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(in_hi, 1));
                dest+=16;
                src+=32;
            }
            #endif
            uint16_t* d = (uint16_t*)(dest);
            for (; i < size; i++) *d++ = *src++ * 4369;
            break;
        }
        case 8: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                __m256i in_hi = _mm256_unpackhi_epi8(in, in);
                _mm_storeu_si128((__m128i*)(dest + i + 0 * 8), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 8), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i + 2 * 8), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i + 3 * 8), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            for (; i < size; i++) dest[i] = src[i] * 257;
            break;
        }
        case 16: {
            free(new_buffer);
            return true;
        }
        default: {
            free(new_buffer);
            return false;
        }
    }

    free(image->buffer);
    image->buffer = new_buffer;

    image->bit_depth = 16;

    return true;
}











struct slp_image_crop_thread_data {
    uint64_t c;
    uint64_t src_stride;
    uint64_t dest_stride;
    uint32_t offset_width;
    uint32_t offset_height;
    struct slp_image *image;
    uint8_t* new_buffer;
    uint64_t block;
    uint64_t last_block;
    int s;
    int P;
};

static void* slp_image_crop_thread_task(void *arg) {
    struct slp_image_crop_thread_data data = *(struct slp_image_crop_thread_data*)arg;
    uint8_t* src = data.image->buffer + (uint64_t)(data.offset_height + data.s * data.block) * data.src_stride + (uint64_t)data.offset_width * data.c;
    uint8_t* dest = data.new_buffer + data.s * data.block * data.dest_stride;
    for (uint64_t i = data.s * data.block; i < data.s * data.block + ((data.s == data.P - 1) ? data.last_block : data.block); i++) memcpy(dest + i * data.dest_stride, src + i * data.src_stride, data.dest_stride);
    return NULL;
}

// mem: O(N)
bool image_crop(struct slp_image *image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height) {
    if (__builtin_expect(offset_width + new_width > image->width || offset_height + new_height > image->height, 0)) return false;

    const uint64_t c = (uint64_t)image->channels * (uint64_t)(1 + (image->bit_depth == 16)); // sizeof 1 pixel
    const uint64_t src_stride = (uint64_t)image->width * c;
    const uint64_t dest_stride = (uint64_t)new_width * c;

    uint8_t* new_buffer = (uint8_t*)malloc(dest_stride * new_height);
    if (__builtin_expect(new_buffer == NULL, 0)) return false;

    const int P = (sysconf(_SC_NPROCESSORS_ONLN) <= 1) ? (2) : (sysconf(_SC_NPROCESSORS_ONLN));

    struct slp_image_crop_thread_data *threads_arg = (struct slp_image_crop_thread_data*)malloc(P * sizeof(*threads_arg));
    if (__builtin_expect(threads_arg == NULL, 0)) {
        free(new_buffer);
        return false;
    }

    pthread_t* threads = (pthread_t*)malloc(P * sizeof(*threads));
    if (__builtin_expect(threads == NULL, 0)) {
        free(new_buffer);
        free(threads_arg);
        return false;
    }

    const uint64_t block = new_height / (P - 1); // P threads, P-1 work for block of scanline, the remain 1 thread works for whatever remains
    const uint64_t last_block = new_height - block * (P-1); // == new_height % (P-1)

    int s = 0;

    for (; s < P-1; s++) {
        threads_arg[s].c = c;
        threads_arg[s].src_stride = src_stride;
        threads_arg[s].dest_stride = dest_stride;
        threads_arg[s].offset_width = offset_width;
        threads_arg[s].offset_height = offset_height;
        threads_arg[s].image = image;
        threads_arg[s].new_buffer = new_buffer;
        threads_arg[s].block = block;
        threads_arg[s].last_block = last_block;
        threads_arg[s].s = s;
        threads_arg[s].P = P;

        if (pthread_create(threads + s, NULL, slp_image_crop_thread_task, threads_arg + s) != 0) {
            for (int i = 0; i < s; i++) {
                pthread_join(threads[i], NULL);
            }
            free(threads);
            free(new_buffer);
            free(threads_arg);
            return false;
        }
    }
    
    threads_arg[s].c = c;
    threads_arg[s].src_stride = src_stride;
    threads_arg[s].dest_stride = dest_stride;
    threads_arg[s].offset_width = offset_width;
    threads_arg[s].offset_height = offset_height;
    threads_arg[s].image = image;
    threads_arg[s].new_buffer = new_buffer;
    threads_arg[s].block = block;
    threads_arg[s].last_block = last_block;
    threads_arg[s].s = s;
    threads_arg[s].P = P;

    if (pthread_create(threads + s, NULL, slp_image_crop_thread_task, threads_arg + s) != 0) {
        for (int i = 0; i < s; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        free(new_buffer);
        free(threads_arg);
        return false;
    }

    for (int i = 0; i < P; i++) pthread_join(threads[i], NULL);

    free(threads);
    free(threads_arg);
    free(image->buffer);

    image->buffer = new_buffer;
    image->width = new_width;
    image->height = new_height;

    return true;
}










// x must >= 0
static int64_t ceil__(double x) {
    int64_t a = (int64_t)x;
    return a + (x > a);
}


static void slp_image_fill(uint8_t* buffer, uint64_t buffer_size, const uint8_t* pixel, const uint8_t pixel_size) {

    memcpy(buffer, pixel, pixel_size);

    uint64_t filled = pixel_size;

    while (filled < 64 && filled < buffer_size) {
        uint64_t copy = (filled < buffer_size - filled) ? filled : (buffer_size - filled);
        memcpy(buffer + filled, buffer, copy);
        filled += copy;
    }

    while (filled < buffer_size) {
        uint64_t copy = (filled < buffer_size - filled) ? filled : (buffer_size - filled);
        memcpy(buffer + filled, buffer, copy);
        filled += copy;
    }
}


struct slp_image_linear_transform_thread_arg {
    struct slp_image *image;
    uint8_t *background;
    double *inverseA;
    double half_height;
    double half_width;
    double umin;
    double vmax;
    uint32_t new_width;
    uint64_t pixel_size;
    uint64_t src_stride;
    uint64_t dst_stride;
    uint8_t* new_buffer;
    uint8_t* src;
    int P;
    uint64_t block;
    uint64_t last_block;
    int s;
    bool* start_execute;
    bool* abort;
};



static void* slp_image_linear_transform_thread_task(void* arg) {

    struct slp_image_linear_transform_thread_arg data = *(struct slp_image_linear_transform_thread_arg*)arg;

    int64_t i = data.s * data.block;

    double c1 = (-i + data.vmax) * data.inverseA[1] + data.half_width;
    double c2 = (-i + data.vmax) * data.inverseA[3] - data.half_height;

    double X = data.umin * data.inverseA[0] + c1;
    double Y = -data.umin * data.inverseA[2] - c2;

    int entry_flagx;
    if (data.inverseA[0] > 0) entry_flagx = 1;
    else if (data.inverseA[0] < 0) entry_flagx = -1;
    else entry_flagx = 0;

    int entry_flagy;
    if (data.inverseA[2] > 0) entry_flagy = 1;
    else if (data.inverseA[2] < 0) entry_flagy = -1;
    else entry_flagy = 0;

    while (!(*data.start_execute)) {
        if (*data.abort == true) return NULL;
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000;//wait for 1 microsec to avoid spamming
        nanosleep(&ts, NULL);
    }

    for (; i < data.s * data.block + ((data.s == data.P - 1) ? data.last_block : data.block); i++) {
        
        uint8_t* dest = data.new_buffer + i * data.dst_stride;

        double x0, x2, y0, y2;

        switch (entry_flagx) {
            case 1: {
                x0 = -X / data.inverseA[0];
                x2 = (data.image->width - X - 1) / data.inverseA[0];
                break;
            }
            case -1: {
                x0 = (data.image->width - X - 1) / data.inverseA[0];
                x2 = -X / data.inverseA[0];
                break;
            }
            case 0: {
                x0 = 0;
                x2 = data.new_width;
                break;
            }
        }

        switch (entry_flagy) {
            case 1: {
                y0 = (Y - data.image->height + 1) / data.inverseA[2];
                y2 = Y / data.inverseA[2];
                break;
            }
            case -1: {
                y0 = Y / data.inverseA[2];
                y2 = (Y - data.image->height + 1) / data.inverseA[2];
                break;
            }
            case 0: {
                y0 = 0;
                y2 = data.new_width;
                break;
            }
        }

        int64_t start = ceil__(max(x0, y0));
        int64_t end = (int64_t)(min(x2, y2));
        
        dest += start * data.pixel_size;

        int64_t mid = start;

        double x1 = mid * data.inverseA[0] + X;
        double y1 = -mid * data.inverseA[2] + Y;
        
        for (; mid < end; mid++) { // data
            memcpy(dest, data.src + ((int64_t)y1) * data.src_stride + ((int64_t)x1) * data.pixel_size, data.pixel_size);
            dest += data.pixel_size;
            x1 += data.inverseA[0];
            y1 += -data.inverseA[2];
        }

        c1 -= data.inverseA[1];
        c2 -= data.inverseA[3];

        X -= data.inverseA[1];
        Y += data.inverseA[3];
    }

    return NULL;
}



bool slp_image_linear_transform(struct slp_image *image, const double* A, const uint8_t* background) {
    // DO NOT USE FLOAT
    const double detA = A[0] * A[3] - A[1] * A[2];
    if (__builtin_expect(detA == 0, 0)) {
        const uint64_t size = (uint64_t)image->width * (uint64_t)image->height * (uint64_t)image->channels * (1 + (image->bit_depth == 16));
        memset(image->buffer, 0, size);
        image->height = 0;
        image->width = 0;
        return true;
    }
    const double inverseA[4] = { A[3] / detA, -A[1] / detA, -A[2] / detA, A[0] / detA };
    // first we will calculate its new size, through the 4 corners
    // 0, (H-1) -> -(H-1)/2, (H-1)/2
    // 0, (W-1) -> -(W-1)/2, (W-1)/2
    double half_height = ((double)image->height-1)/2;// y
    double half_width = ((double)image->width-1)/2;// x

    // 1    2
    // 3    4
    const double u1 = -half_width * A[0] + half_height * A[1];
    const double u2 = half_width * A[0] + half_height * A[1];
    const double u3 = -half_width * A[0] + -half_height * A[1];
    const double u4 = half_width * A[0] + -half_height * A[1];

    const double v1 = -half_width * A[2] + half_height * A[3];
    const double v2 = half_width * A[2] + half_height * A[3];
    const double v3 = -half_width * A[2] + -half_height * A[3];
    const double v4 = half_width * A[2] + -half_height * A[3];

    const double umax = max(max(max(u1, u2), u3), u4);
    const double umin = min(min(min(u1, u2), u3), u4);
    const double vmax = max(max(max(v1, v2), v3), v4);
    const double vmin = min(min(min(v1, v2), v3), v4);

    const uint32_t new_width = (uint32_t)(umax - umin + 1);
    const uint32_t new_height = (uint32_t)(vmax - vmin + 1);

    const uint64_t pixel_size = image->channels * (1 + (image->bit_depth == 16)); // sizeof 1 pixel
    const uint64_t src_stride = image->width * pixel_size; // sizeof 1 scanline
    const uint64_t dst_stride = new_width * pixel_size; // sizeof 1 dest scanline
    const uint64_t new_size = (uint64_t)new_width * (uint64_t)new_height * pixel_size;

    uint8_t* new_buffer = (uint8_t*)malloc(new_size);
    if (new_buffer == NULL) {
        return false;
    }

    slp_image_fill(new_buffer, new_size, background, pixel_size);

    uint8_t* src = image->buffer;

    const int P = (sysconf(_SC_NPROCESSORS_ONLN) <= 1) ? (2) : (sysconf(_SC_NPROCESSORS_ONLN));

    pthread_t *threads = (pthread_t*)malloc(P * sizeof(*threads));
    if (__builtin_expect(threads == NULL, 0)) {
        free(new_buffer);
        return false;
    }

    struct slp_image_linear_transform_thread_arg *threads_arg = (struct slp_image_linear_transform_thread_arg*)malloc(P * sizeof(*threads_arg));
    if (__builtin_expect(threads_arg == NULL, 0)) {
        free(new_buffer);
        free(threads);
        return false;
    }

    const uint64_t block = new_height / (P - 1); // P threads, P-1 work for block of scanline, the remain 1 thread works for what remain:)
    const uint64_t last_block = new_height - block * (P-1); // == new_height % (P-1)
    
    int s = 0;

    bool thread_start_execute = false;
    bool thread_abort = false;

    for (; s < P-1; s++) {
        threads_arg[s].image = image;
        threads_arg[s].background = (uint8_t*)background;
        threads_arg[s].inverseA = (double*)inverseA;
        threads_arg[s].half_height = half_height;
        threads_arg[s].half_width = half_width;
        threads_arg[s].umin = umin;
        threads_arg[s].vmax = vmax;
        threads_arg[s].new_width = new_width;
        threads_arg[s].pixel_size = pixel_size;
        threads_arg[s].src_stride = src_stride;
        threads_arg[s].dst_stride = dst_stride;
        threads_arg[s].new_buffer = new_buffer;
        threads_arg[s].src = src;
        threads_arg[s].P = P;
        threads_arg[s].block = block;
        threads_arg[s].last_block = last_block;
        threads_arg[s].s = s;
        threads_arg[s].start_execute = &thread_start_execute;
        threads_arg[s].abort = &thread_abort;
        
        if (pthread_create(threads + s, NULL, slp_image_linear_transform_thread_task, threads_arg + s) != 0) {
            thread_abort = true;
            for (int i = 0; i < s; i++) {
                pthread_join(threads[i], NULL);
            }
            free(threads);
            free(new_buffer);
            free(threads_arg);
            return false;
        }
    }

    threads_arg[s].image = image;
    threads_arg[s].background = (uint8_t*)background;
    threads_arg[s].inverseA = (double*)inverseA;
    threads_arg[s].half_height = half_height;
    threads_arg[s].half_width = half_width;
    threads_arg[s].umin = umin;
    threads_arg[s].vmax = vmax;
    threads_arg[s].new_width = new_width;
    threads_arg[s].pixel_size = pixel_size;
    threads_arg[s].src_stride = src_stride;
    threads_arg[s].dst_stride = dst_stride;
    threads_arg[s].new_buffer = new_buffer;
    threads_arg[s].src = src;
    threads_arg[s].P = P;
    threads_arg[s].block = block;
    threads_arg[s].last_block = last_block;
    threads_arg[s].s = s;
    threads_arg[s].start_execute = &thread_start_execute;
    threads_arg[s].abort = &thread_abort;
    
    if (pthread_create(threads + s, NULL, slp_image_linear_transform_thread_task, threads_arg + s) != 0) {
        thread_abort = true;
        for (int i = 0; i < s; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        free(new_buffer);
        free(threads_arg);
        return false;
    }

    thread_start_execute = true;
    for (int i = 0; i < P; i++) pthread_join(threads[i], NULL);

    free(threads);
    free(threads_arg);
    free(image->buffer);

    image->buffer = new_buffer;
    image->width = new_width;
    image->height = new_height;

    return true;
}








// only take microseconds to run
bool slp_image_format(struct slp_image *image) {

    const uint64_t size = (uint64_t)image->width * (uint64_t)image->height * (uint64_t)image->channels * (uint64_t)(1 + (image->bit_depth == 16));

    uint8_t *new_buffer = (uint8_t*)malloc(size);
    if (__builtin_expect(new_buffer == NULL, 0)) {
        if (image->bit_depth == 8) return true;
        return false;
    }

    uint8_t *src = image->buffer;
    uint8_t *dest = new_buffer;


    switch (image->bit_depth) {
        case 1: {
            uint64_t i = 0;
            #ifdef __SSE2__
            __m128i ones = _mm_set1_epi16(1);
            __m128i zeroes = _mm_setzero_si128();

            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i *)src);

                __m128i in_lo = _mm_unpacklo_epi8(in, zeroes);
                __m128i in_hi = _mm_unpackhi_epi8(in, zeroes);

                __m128i in0_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 0), ones);
                __m128i in1_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 1), ones);
                __m128i in2_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 2), ones);
                __m128i in3_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 3), ones);
                __m128i in4_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 4), ones);
                __m128i in5_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 5), ones);
                __m128i in6_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 6), ones);
                __m128i in7_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 7), ones);

                __m128i in0_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 0), ones);
                __m128i in1_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 1), ones);
                __m128i in2_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 2), ones);
                __m128i in3_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 3), ones);
                __m128i in4_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 4), ones);
                __m128i in5_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 5), ones);
                __m128i in6_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 6), ones);
                __m128i in7_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 7), ones);

                __m128i in0 = _mm_packus_epi16(in0_lo, in0_hi);
                __m128i in1 = _mm_packus_epi16(in1_lo, in1_hi);
                __m128i in2 = _mm_packus_epi16(in2_lo, in2_hi);
                __m128i in3 = _mm_packus_epi16(in3_lo, in3_hi);
                __m128i in4 = _mm_packus_epi16(in4_lo, in4_hi);
                __m128i in5 = _mm_packus_epi16(in5_lo, in5_hi);
                __m128i in6 = _mm_packus_epi16(in6_lo, in6_hi);
                __m128i in7 = _mm_packus_epi16(in7_lo, in7_hi);

                // 16 bit & 8 lanes
                //  8 lanes will interleave by pair so 16 bit 8 lanes to 32 bit 4 lanes to 64 bit 2 lanes
                //  lane is a block of bytes like a chunk, after 8 lanes - > 4 lanes we have group some elements in order as
                //  we want storing elements still working like normal 64-bit does not means an element is now 64 bit it
                //  means 1 lane/chunk is 64 bit

                __m128i a0 = _mm_unpacklo_epi8(in0, in1); // 0 x 1 lo
                __m128i a1 = _mm_unpackhi_epi8(in0, in1); // 0 x 1 hi
                __m128i a2 = _mm_unpacklo_epi8(in2, in3); // 2 x 3 lo
                __m128i a3 = _mm_unpackhi_epi8(in2, in3); // 2 x 3 hi
                __m128i a4 = _mm_unpacklo_epi8(in4, in5); // 4 x 5 lo
                __m128i a5 = _mm_unpackhi_epi8(in4, in5); // 4 x 5 hi
                __m128i a6 = _mm_unpacklo_epi8(in6, in7); // 6 x 7 lo
                __m128i a7 = _mm_unpackhi_epi8(in6, in7); // 6 x 7 hi


                __m128i b0 = _mm_unpacklo_epi16(a0, a2); // 01 lo x 23 lo p0
                __m128i b1 = _mm_unpacklo_epi16(a1, a3); // 01 hi x 23 hi p2
                __m128i b2 = _mm_unpacklo_epi16(a4, a6); // 45 lo x 67 lo p0
                __m128i b3 = _mm_unpacklo_epi16(a5, a7); // 45 hi x 67 hi p2

                __m128i b4 = _mm_unpackhi_epi16(a0, a2); // 01 lo x 23 lo p1
                __m128i b5 = _mm_unpackhi_epi16(a1, a3); // 01 hi x 23 hi p3
                __m128i b6 = _mm_unpackhi_epi16(a4, a6); // 45 lo x 67 lo p1
                __m128i b7 = _mm_unpackhi_epi16(a5, a7); // 45 hi x 67 hi p3


                __m128i c0 = _mm_unpacklo_epi32(b0, b2); // 0123 p0 x 4567 p0 pp0
                __m128i c1 = _mm_unpacklo_epi32(b4, b6); // 0123 p1 x 4567 p1 pp2
                __m128i c2 = _mm_unpacklo_epi32(b1, b3); // 0123 p2 x 4567 p2 pp4
                __m128i c3 = _mm_unpacklo_epi32(b5, b7); // 0123 p3 x 4567 p3 pp6

                __m128i c4 = _mm_unpackhi_epi32(b0, b2); // 0123 p0 x 4567 p0 pp1
                __m128i c5 = _mm_unpackhi_epi32(b4, b6); // 0123 p1 x 4567 p1 pp3
                __m128i c6 = _mm_unpackhi_epi32(b1, b3); // 0123 p2 x 4567 p2 pp5
                __m128i c7 = _mm_unpackhi_epi32(b5, b7); // 0123 p3 x 4567 p3 pp7

                _mm_storeu_si128((__m128i *)(dest + 0 * 16), c0);
                _mm_storeu_si128((__m128i *)(dest + 1 * 16), c4);
                _mm_storeu_si128((__m128i *)(dest + 2 * 16), c1);
                _mm_storeu_si128((__m128i *)(dest + 3 * 16), c5);
                _mm_storeu_si128((__m128i *)(dest + 4 * 16), c2);
                _mm_storeu_si128((__m128i *)(dest + 5 * 16), c6);
                _mm_storeu_si128((__m128i *)(dest + 6 * 16), c3);
                _mm_storeu_si128((__m128i *)(dest + 7 * 16), c7);

                src += 16;
                dest += 128;
            }
            #endif
            for (; i < size; i++) {
                for (int j = 7; j >= 0; j--) *dest++ = ((*src >> j) & 1);
                src++;
            }
            break;
        }
        case 2: {
            uint64_t i = 0;
            #ifdef __SSE2__
            __m128i ones = _mm_set1_epi16(0b11);
            __m128i zeroes = _mm_setzero_si128();

            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i *)src);

                __m128i in_lo = _mm_unpacklo_epi8(in, zeroes);
                __m128i in_hi = _mm_unpacklo_epi8(in, zeroes);

                __m128i in0_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 0), ones);
                __m128i in0_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 0), ones);
                __m128i in1_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 2), ones);
                __m128i in1_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 2), ones);
                __m128i in2_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 4), ones);
                __m128i in2_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 4), ones);
                __m128i in3_lo = _mm_and_si128(_mm_srli_epi16(in_lo, 6), ones);
                __m128i in3_hi = _mm_and_si128(_mm_srli_epi16(in_hi, 6), ones);

                __m128i in0 = _mm_packus_epi16(in0_lo, in0_hi); // 0
                __m128i in1 = _mm_packus_epi16(in1_lo, in1_hi); // 1
                __m128i in2 = _mm_packus_epi16(in2_lo, in2_hi); // 2
                __m128i in3 = _mm_packus_epi16(in3_lo, in3_hi); // 3

                __m128i a0 = _mm_unpacklo_epi8(in0, in1); // 0 lo x 1 lo p0
                __m128i a1 = _mm_unpackhi_epi8(in0, in1); // 0 hi x 1 hi p1
                __m128i a2 = _mm_unpacklo_epi8(in2, in3); // 2 lo x 3 lo p0
                __m128i a3 = _mm_unpackhi_epi8(in2, in3); // 2 hi x 3 hi p1

                __m128i b0 = _mm_unpacklo_epi16(a0, a2); // 01 p0 x 23 p0 pp0
                __m128i b1 = _mm_unpacklo_epi16(a1, a3); // 01 p1 x 23 p1 pp2
                __m128i b2 = _mm_unpackhi_epi16(a0, a2); // 01 p0 x 23 p0 pp1
                __m128i b3 = _mm_unpackhi_epi16(a1, a3); // 01 p1 x 23 p1 pp3

                _mm_storeu_si128((__m128i *)(dest + 0 * 16), b0);
                _mm_storeu_si128((__m128i *)(dest + 1 * 16), b2);
                _mm_storeu_si128((__m128i *)(dest + 2 * 16), b1);
                _mm_storeu_si128((__m128i *)(dest + 3 * 16), b3);

                src += 16;
                dest += 64;
            }
            #endif
            for (; i < size; i++) {
                for (int j = 3; j >= 0; j--) { *dest++ = ((*src >> (j * 2)) & 3); }
                src++;
            }
            break;
        }
        case 4: {
            uint64_t i = 0;
            #ifdef __AVX2__
            __m256i ones = _mm256_set1_epi16(0b1111);
            __m256i zeroes = _mm256_setzero_si256();

            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i *)src);

                __m256i in_lo = _mm256_unpacklo_epi8(in, zeroes);
                __m256i in_hi = _mm256_unpackhi_epi8(in, zeroes);
                
                __m256i in0_lo = _mm256_and_si256(_mm256_srli_epi16(in_lo, 0), ones);
                __m256i in0_hi = _mm256_and_si256(_mm256_srli_epi16(in_hi, 0), ones);
                __m256i in1_lo = _mm256_and_si256(_mm256_srli_epi16(in_lo, 4), ones);
                __m256i in1_hi = _mm256_and_si256(_mm256_srli_epi16(in_hi, 4), ones);
                
                __m256i in0 = _mm256_packus_epi16(in0_lo, in0_hi);
                __m256i in1 = _mm256_packus_epi16(in1_lo, in1_hi);
                
                __m256i a0 = _mm256_unpacklo_epi8(in0, in1); // 0 x 1 lo
                __m256i a1 = _mm256_unpackhi_epi8(in0, in1); // 0 x 1 hi

                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(a0));
                dest+=16;

                _mm_storeu_si128((__m128i*)dest, _mm256_castsi256_si128(a1));
                dest+=16;

                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(a0, 1));
                dest+=16;

                _mm_storeu_si128((__m128i*)dest, _mm256_extracti128_si256(a1, 1));
                dest+=16;
                
                src += 32;
            }
            #endif
            for (; i < size; i++) {
                for (int j = 1; j >= 0; j--) { *dest++ = ((*src >> (j * 4)) & 0b1111); }
                src++;
            }
            break;
        }
        case 8: {
            free(new_buffer);
            return true;
        }
        case 16: {
            uint16_t random_value_for_edian_test = 1;
            if (!(*(uint8_t*)(&random_value_for_edian_test))) {// if big edian
                free(new_buffer);
                return true;
            }

            uint64_t i = 0;
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i *)(src + i));
                __m256i out = _mm256_or_si256(_mm256_slli_epi16(in, 8), _mm256_srli_epi16(in, 8));
                _mm256_storeu_si256((__m256i *)(dest + i), out);
            }
            #endif
            for (; i < size; i+=2) {
                dest[i + 0] = src[i + 1];
                dest[i + 1] = src[i + 0];
            }
            break;
        }
        default: {
            free(new_buffer);
            return false;
        }
    }

    free(image->buffer);
    image->buffer = new_buffer;

    return true;
}








// only take microseconds to run
bool slp_image_unformat(struct slp_image *image) {

    const uint64_t size = (uint64_t)(image->height) * (uint64_t)(image->width) * (uint64_t)(image->channels) * (uint64_t)(1 + (image->bit_depth == 16));
    const uint64_t new_size = (uint64_t)(image->height) * ceil__(((double)(image->width) * (double)(image->channels) * (double)(image->bit_depth)) / 8.0);
    
    uint8_t* new_buffer = (uint8_t*)malloc(new_size);
    if (new_buffer == NULL) return false;

    uint8_t *src = (uint8_t*)(image->buffer);
    uint8_t *dest = (uint8_t*)(new_buffer);

    uint64_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            __m256i mask1 = _mm256_setr_epi8(1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
            __m256i mask2 = _mm256_setr_epi8(0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0);
            __m256i mask3 = _mm256_setr_epi8(0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
            __m256i mask4 = _mm256_setr_epi8(0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0);
            __m256i mask5 = _mm256_setr_epi8(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
            __m256i mask6 = _mm256_setr_epi8(0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
            __m256i mask7 = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0);
            __m256i mask8 = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);
            __m256i extract = _mm256_setr_epi8(0, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);


            __m256i z = _mm256_setzero_si256();
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i *)src);

                in = _mm256_and_si256(in, _mm256_set1_epi8(1)); // mask the last 1 bit of each byte

                __m256i bit1 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask1), 7), 0);
                __m256i bit2 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask2), 6), 1);
                __m256i bit3 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask3), 5), 2);
                __m256i bit4 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask4), 4), 3);
                __m256i bit5 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask5), 3), 4);
                __m256i bit6 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask6), 2), 5);
                __m256i bit7 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask7), 1), 6);
                __m256i bit8 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_blendv_epi8(z, in, mask8), 0), 7);

                __m256i out = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(bit1, bit2), bit3), bit4), bit5), bit6), bit7), bit8);

                out = _mm256_shuffle_epi8(out, extract);

                *(uint16_t *)dest = _mm256_extract_epi16(out, 0);
                dest += 2;
                *(uint16_t *)dest = _mm256_extract_epi16(out, 8);
                dest += 2;
                src += 32;
            }
            #endif
            for (; i < size; i++) {
                *dest = 0;
                for (uint64_t j = 7; j >= 0; j++) *dest |= (((*src++) & 1) << j);
                dest++;
            }
            break;
        }
        case 2: {
            #ifdef __AVX2__
            __m256i mask1 = _mm256_setr_epi8(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0);
            __m256i mask2 = _mm256_setr_epi8(0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0);
            __m256i mask3 = _mm256_setr_epi8(0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0);
            __m256i mask4 = _mm256_setr_epi8(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1);
            __m256i extract = _mm256_setr_epi8(0, 4, 8, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 4, 8, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

            __m256i z = _mm256_setzero_si256();
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i *)src);

                in = _mm256_and_si256(in, _mm256_set1_epi8(0b11)); // mask the last 2 bit of each byte

                __m256i bit1 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_blendv_epi8(z, in, mask1), 3), 0);
                __m256i bit2 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_blendv_epi8(z, in, mask2), 2), 1);
                __m256i bit3 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_blendv_epi8(z, in, mask3), 1), 2);
                __m256i bit4 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_blendv_epi8(z, in, mask4), 0), 3);

                __m256i out = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(bit1, bit2), bit3), bit4);

                out = _mm256_shuffle_epi8(out, extract);

                *(uint32_t *)dest = _mm256_extract_epi32(out, 0);
                dest += 4;
                *(uint32_t *)dest = _mm256_extract_epi32(out, 4);
                dest += 4;
                src += 32;
            }
            #endif
            for (; i < size; i++) {
                *dest = 0;
                for (uint64_t j = 3; j >= 0; j++) *dest |= (((*src++) & 0b11) << j);
                dest++;
            }
            break;
        }
        case 4: {
            #ifdef __AVX2__
            __m256i mask1 = _mm256_setr_epi8(1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0);
            __m256i mask2 = _mm256_setr_epi8(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1);
            __m256i extract = _mm256_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1, 0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1);

            __m256i z = _mm256_setzero_si256();
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i *)src);

                in = _mm256_and_si256(in, _mm256_set1_epi8(0b1111)); // mask the last 4 bit of each byte

                __m256i bit1 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_blendv_epi8(z, in, mask1), 1), 0);
                __m256i bit2 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_blendv_epi8(z, in, mask2), 0), 1);

                __m256i out = _mm256_or_si256(bit1, bit2);

                out = _mm256_shuffle_epi8(out, extract);

                *(uint64_t *)dest = _mm256_extract_epi64(out, 0);
                dest += 8;
                *(uint64_t *)dest = _mm256_extract_epi64(out, 2);
                dest += 8;
                src += 32;
            }
            #endif
            for (; i < size; i++) {
                *dest = 0;
                for (uint64_t j = 1; j >= 0; j++) *dest |= (((*src++) & 0b1111) << j);
                dest++;
            }
            break;
        }
        case 8: {
            free(new_buffer);
            return true;
            break;
        }
        case 16: {
            uint16_t random_value_for_edian_test = 1;
            if (!(*(uint8_t*)(&random_value_for_edian_test))) {
                free(new_buffer);
                return true;
            }

            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i *)(src + i));
                __m256i out = _mm256_or_si256(_mm256_slli_epi16(in, 8), _mm256_srli_epi16(in, 8));
                _mm256_storeu_si256((__m256i *)(dest + i), out);
            }
            #endif
            for (; i + 2 <= size; i+=2) {
                dest[i + 0] = src[i + 1];
                dest[i + 1] = src[i + 0];
            }
            break;
        }
        default: {
            free(new_buffer);
            return false;
        }
    }

    free(image->buffer);
    image->buffer = new_buffer;

    return true;
}









// did not test, not sure if work reliably
bool slp_image_convert_G8_to_RGBA32(struct slp_image *image) {
    if (image->channels != 1 || image->bit_depth != 8) return false;

    const uint64_t size = (uint64_t)image->width * (uint64_t)image->height;

    uint8_t *new_buffer = (uint8_t*)malloc((uint64_t)image->width * (uint64_t)image->height * 4);
    if (new_buffer == NULL) {
        return false;
    }

    uint8_t *src = image->buffer;
    uint8_t *dest = new_buffer;

    uint64_t i = 0;
    #ifdef __SSE2__
    __m128i FF = _mm_set1_epi8(0xFF);
    for (; i + 16 <= size; i+=16) {
        __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

        __m128i inin1 = _mm_unpacklo_epi8(in, in);
        __m128i inin2 = _mm_unpackhi_epi8(in, in);

        __m128i inFF1 = _mm_unpacklo_epi8(in, FF);
        __m128i inFF2 = _mm_unpackhi_epi8(in, FF);

        __m128i inininFF_lo1 = _mm_unpacklo_epi16(inin1, inFF1);
        __m128i inininFF_hi1 = _mm_unpackhi_epi16(inin1, inFF1);

        __m128i inininFF_lo2 = _mm_unpacklo_epi16(inin2, inFF2);
        __m128i inininFF_hi2 = _mm_unpackhi_epi16(inin2, inFF2);

        _mm_storeu_si128((__m128i*)(dest + i + 16 * 0), inininFF_lo1);
        _mm_storeu_si128((__m128i*)(dest + i + 16 * 1), inininFF_hi1);
        _mm_storeu_si128((__m128i*)(dest + i + 16 * 2), inininFF_lo2);
        _mm_storeu_si128((__m128i*)(dest + i + 16 * 3), inininFF_hi2);
    }
    #endif
    for (; i < size; i++) {
        dest[i] = src[i];//R
        dest[i] = src[i];//G
        dest[i] = src[i];//B
        dest[i+1] = 0xFF;//A
    }

    free(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;

    return true;
}


// did not test, not sure if work reliably
bool slp_image_convert_GA16_to_RGBA32(struct slp_image *image) {
    if (image->channels != 2 || image->bit_depth != 8) return false;

    const uint64_t src_size_in_element = (uint64_t)image->width * (uint64_t)image->height * 2;// size in element

    uint8_t *new_buffer = (uint8_t*)malloc((uint64_t)image->width * (uint64_t)image->height * 8);
    if (new_buffer == NULL) return false;

    uint16_t *src = (uint16_t*)image->buffer;
    uint16_t *dest = (uint16_t*)new_buffer;

    uint64_t i = 0;
    #ifdef __SSE2__
    __m128i FF00 = _mm_set1_epi16(0xFF);
    for (; i + 8 <= src_size_in_element; i+=8) {
        __m128i inAA = _mm_loadu_si128((const __m128i*)(src + i));

        __m128i in00 = _mm_and_si128(inAA, FF00);
        __m128i _00in = _mm_srli_epi16(inAA, 8);

        __m128i inin = _mm_or_si128(in00, _00in);

        __m128i inininAA_lo = _mm_unpacklo_epi16(inin, inAA);
        __m128i inininAA_hi = _mm_unpackhi_epi16(inin, inAA);

        _mm_storeu_si128((__m128i*)(dest + i + 8 * 0), inininAA_lo);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 1), inininAA_hi);
    }
    #endif
    for (; i < src_size_in_element; i+=2) {
        dest[i] = src[i];//R
        dest[i] = src[i];//G
        dest[i] = src[i];//B
        dest[i+1] = src[i+1];//A
    }

    free(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;

    return true;
}


// did not test, not sure if work reliably
bool slp_image_convert_G16_to_RGBA64(struct slp_image *image) {
    if (image->channels != 1 || image->bit_depth != 16) return false;

    const uint64_t src_size_in_element = (uint64_t)image->width * (uint64_t)image->height * 2;

    uint8_t *new_buffer = (uint8_t*)malloc((uint64_t)image->width * (uint64_t)image->height * 8);
    if (new_buffer == NULL) {
        return false;
    }

    uint16_t *src = (uint16_t*)image->buffer;
    uint16_t *dest = (uint16_t*)new_buffer;

    uint64_t i = 0;
    #ifdef __SSE2__
    __m128i FF = _mm_set1_epi16(0xFFFF);
    for (; i + 8 <= src_size_in_element; i+=8) {
        __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

        __m128i inin1 = _mm_unpacklo_epi16(in, in);
        __m128i inin2 = _mm_unpackhi_epi16(in, in);

        __m128i inFF1 = _mm_unpacklo_epi16(in, FF);
        __m128i inFF2 = _mm_unpackhi_epi16(in, FF);

        __m128i inininFF_lo1 = _mm_unpacklo_epi32(inin1, inFF1);
        __m128i inininFF_lo2 = _mm_unpacklo_epi32(inin2, inFF2);

        __m128i inininFF_hi1 = _mm_unpackhi_epi32(inin1, inFF1);
        __m128i inininFF_hi2 = _mm_unpackhi_epi32(inin2, inFF2);

        _mm_storeu_si128((__m128i*)(dest + i + 8 * 0), inininFF_lo1);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 1), inininFF_hi1);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 2), inininFF_lo2);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 3), inininFF_hi2);
    }
    #endif
    for (; i < src_size_in_element; i++) {
        dest[i + 0] = src[i];
        dest[i + 1] = src[i];
        dest[i + 2] = src[i];
        dest[i + 4] = 0xFFFF;
    }

    free(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;

    return true;
}

// did not test, not sure if work reliably
bool slp_image_convert_GA32_to_RGBA64(struct slp_image *image) {
    if (image->channels != 2 || image->bit_depth != 16) return false;

    const uint64_t src_size_in_element = (uint64_t)image->width * (uint64_t)image->height * 4;

    uint8_t *new_buffer = (uint8_t*)malloc((uint64_t)image->width * (uint64_t)image->height * 8);
    if (new_buffer == NULL) {
        return false;
    }

    uint16_t *src = (uint16_t*)image->buffer;
    uint16_t *dest = (uint16_t*)new_buffer;

    uint64_t i = 0;
    #ifdef __SSE2__
    __m128i FF00 = _mm_set1_epi32(0xFFFF);
    for (; i + 8 <= src_size_in_element; i+=8) {
        __m128i inAA = _mm_loadu_si128((const __m128i*)(src + i));

        __m128i in00 = _mm_and_si128(inAA, FF00);
        __m128i _00in = _mm_srli_epi16(inAA, 16);

        __m128i inin = _mm_or_si128(in00, _00in);

        __m128i inininAA_lo = _mm_unpacklo_epi32(inin, inAA);
        __m128i inininAA_hi = _mm_unpackhi_epi32(inin, inAA);

        _mm_storeu_si128((__m128i*)(dest + i + 8 * 0), inininAA_lo);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 1), inininAA_hi);
    }
    #endif
    for (; i < src_size_in_element; i+=2) {
        dest[i + 0] = src[i];
        dest[i + 1] = src[i];
        dest[i + 2] = src[i];
        dest[i + 4] = src[i + 1];
    }

    free(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;

    return true;
}







struct slp_image slp_image_copy(struct slp_image image) {
    const uint64_t size = (uint64_t)image.width * (uint64_t)image.height * (uint64_t)image.channels * (uint64_t)(1 + (image.bit_depth == 16));
    uint8_t* new_buffer = (uint8_t*)malloc(size);
    if (new_buffer == NULL) {
        image.buffer = NULL;
        return image;
    }
    memcpy(new_buffer, image.buffer, size);
    image.buffer = new_buffer;
    return image;
}




