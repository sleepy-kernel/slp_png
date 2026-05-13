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
#include <slp_png_write.h>
#include <slp_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#include <zlib-ng.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#define __bswap_constant_32(x)                                 \
  ((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >>  8) | \
   (((x) & 0x0000ff00u) <<  8) | (((x) & 0x000000ffu) << 24))

#define edian_swap_u32(x, is_little_edian) ((is_little_edian) ? (__bswap_constant_32(x)) : (x))
//#include <time.h>


// only use for write IHDR
struct IHDR {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
}__attribute__((__packed__));








// helper
// functions

static uint8_t slp_get_color_type(const uint8_t channels);

static int slp_png_encode(struct slp_image *image, FILE* file);

static uint64_t ceil__(double x);












// constants
static const unsigned char PNGsig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
static const unsigned char IHDRsig[4] = {'I', 'H', 'D', 'R'};
static const unsigned char IDATsig[4] = {'I', 'D', 'A', 'T'};
static const unsigned char IENDsig[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};
static const int level = 6; // level of compression
static const int CHUNK = 65536;// just use sth that fit well on cache, fwrite blocking or not depends more on the kernel














// main function imsave:

int slp_png_write(struct slp_image image, const char* path) {

    uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

    if (image.height == 0 || image.width == 0 || image.channels == 0) return 2;

    switch (image.bit_depth) {
        case 1: break;
        case 2: break;
        case 4: break;
        case 8: break;
        case 16: break;
        default: return 2;
    }

    FILE *file = fopen(path, "wb");
    if (__builtin_expect(file == NULL, 0)) return 1;

    struct IHDR header = {0};
    
    header.width = edian_swap_u32(image.width, is_little_edian);
    header.height = edian_swap_u32(image.height, is_little_edian);
    header.bit_depth = image.bit_depth;
    header.color_type = slp_get_color_type(image.channels);
    header.compression_method = 0;
    header.filter_method = 0;
    header.interlace_method = 0;

    if (header.color_type == 0xFF) {
        fclose(file);
        return 2;
    }

    uint32_t data_len = edian_swap_u32(13, is_little_edian);
    if (__builtin_expect(fwrite(PNGsig, 1, 8, file) != 8, 0)) {
        fclose(file);
        return 1;
    }
    
    if (__builtin_expect(fwrite(&data_len, 1, 4, file) != 4, 0)) {
        fclose(file);
        return 1;
    }

    if (__builtin_expect(fwrite(IHDRsig, 1, 4, file) != 4, 0)) {
        fclose(file);
        return 1;
    }

    if (__builtin_expect(fwrite(&header, 1, sizeof(header), file) != 13, 0)) {
        fclose(file);
        return 1;
    }

    uint32_t crc_ = zng_crc32(0, Z_NULL, 0);
    crc_ = zng_crc32(crc_, IHDRsig, 4);
    crc_ = zng_crc32(crc_, (uint8_t*)(&header), 13);
    crc_ = edian_swap_u32(crc_, is_little_edian);

    if (__builtin_expect(fwrite(&crc_, 1, 4, file) != 4, 0)) {
        fclose(file);
        return 1;
    }

    int ret = slp_png_encode(&image, file);
    if (ret != 0) {
        fclose(file);
        return ret;
    }

    fclose(file);
    return 0;
}

















static inline uint8_t slp_get_color_type(const uint8_t channels) {
    switch (channels) {
        case 1: return 0;
        case 2: return 4;
        case 3: return 2;
        case 4: return 6;
        default: return 0xFF;
    }
}











static inline int slp_png_encode(struct slp_image *image, FILE* file) {
    //double deflate_runtime = 0;
    // initialize variables
    int return_code = 0;

    uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

    const uint64_t width = image->width;
    const uint64_t height = image->height;
    const uint64_t channels = image->channels;
    const uint64_t bit_depth = image->bit_depth;

    const uint64_t bpp = channels * (uint64_t)(1 + (bit_depth == 16));
    const uint64_t bpr = ceil__(((double)width * channels * bit_depth) / 8.0);// bytes per row

    uint64_t have = 0;
    uint64_t data_len = 0;
    uint8_t filter_type = 1;

    // pointers declare
    int8_t *filter_buffers[5] = {NULL, NULL, NULL, NULL, NULL};

    uint8_t* image_buffer = image->buffer;

    uint8_t *out = NULL;
    // end pointers declare

    out = (uint8_t*)malloc(CHUNK+12);if (out == NULL) goto cleanup;
    for (int i = 0; i < 5; i++) {
        filter_buffers[i] = (int8_t*)malloc(bpr + 1);
        if (filter_buffers[i] == NULL) goto cleanup;
    }

    memcpy(out + 4, IDATsig, 4);
    filter_buffers[0][0] = 0;
    filter_buffers[1][0] = 1;
    filter_buffers[2][0] = 2;
    filter_buffers[3][0] = 3;
    filter_buffers[4][0] = 4;
    // finish initialize variables








    // CHUNK BEFORE IDAT STAY HERE












    // writting IDAT
    zng_stream strm = {0};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = zng_deflateInit2(&strm, level, Z_DEFLATED, 15, 8, Z_FILTERED);
    if (__builtin_expect(ret != Z_OK, 0)) {
        return_code = 3;
        goto cleanup;
    }
    strm.avail_out = CHUNK;
    for (uint64_t i = 0; i < height; i++)
    {
        int64_t filter_scores[5] = {0};

        if (__builtin_expect(i == 0, 0))
        {
            uint8_t *raw = image_buffer;

            for (uint64_t j = 0; j < bpp; j++) filter_buffers[1][j+1] = raw[j];

            for (uint64_t j = bpp; j < bpr; j++) filter_buffers[1][j+1] = raw[j] - raw[j-bpp];

            for (int j = 0; j < 5; j++) filter_scores[i] = 1000;
            filter_scores[1] = 0;
        }
        else {
            uint8_t *raw = image_buffer + i * bpr;

            uint64_t j = 0;
            for (; j < bpp; j++) {
                filter_buffers[0][j+1] = raw[j];
                filter_buffers[1][j+1] = raw[j];
                filter_buffers[2][j+1] = raw[j] - raw[j - bpr];
                filter_buffers[3][j+1] = raw[j] - (raw[j - bpr]>>1);
                filter_buffers[4][j+1] = raw[j] - raw[j - bpr];

                filter_scores[0] += abs(filter_buffers[0][j+1]);
                filter_scores[1] += abs(filter_buffers[1][j+1]);
                filter_scores[2] += abs(filter_buffers[2][j+1]);
                filter_scores[3] += abs(filter_buffers[3][j+1]);
                filter_scores[4] += abs(filter_buffers[4][j+1]);
            }

            #ifdef __AVX2__
            __m256i noneSum = _mm256_setzero_si256();
            __m256i subSum = _mm256_setzero_si256();
            __m256i upSum = _mm256_setzero_si256();
            __m256i avgSum = _mm256_setzero_si256();
            __m256i paethSum = _mm256_setzero_si256();
            __m256i zero = _mm256_setzero_si256();
            __m256i all1 = _mm256_set1_epi16(-1);


            for (; j + 32 <= bpr; j += 32) {

                __m256i r  = _mm256_loadu_si256((const __m256i *)(raw + j));
                __m256i va = _mm256_loadu_si256((const __m256i *)(raw + j - bpp));
                __m256i vb = _mm256_loadu_si256((const __m256i *)(raw + j - bpr));
                __m256i vc = _mm256_loadu_si256((const __m256i *)(raw + j - bpr - bpp));


                __m256i va_lo = _mm256_unpacklo_epi8(va, zero);
                __m256i va_hi = _mm256_unpackhi_epi8(va, zero);

                __m256i vb_lo = _mm256_unpacklo_epi8(vb, zero);
                __m256i vb_hi = _mm256_unpackhi_epi8(vb, zero);

                __m256i vc_lo = _mm256_unpacklo_epi8(vc, zero);
                __m256i vc_hi = _mm256_unpackhi_epi8(vc, zero);


                __m256i p_lo = _mm256_add_epi16(va_lo, _mm256_sub_epi16(vb_lo, vc_lo));
                __m256i p_hi = _mm256_add_epi16(va_hi, _mm256_sub_epi16(vb_hi, vc_hi));

                __m256i pa_lo = _mm256_abs_epi16(_mm256_sub_epi16(p_lo, va_lo));
                __m256i pa_hi = _mm256_abs_epi16(_mm256_sub_epi16(p_hi, va_hi));

                __m256i pb_lo = _mm256_abs_epi16(_mm256_sub_epi16(p_lo, vb_lo));
                __m256i pb_hi = _mm256_abs_epi16(_mm256_sub_epi16(p_hi, vb_hi));

                __m256i pc_lo = _mm256_abs_epi16(_mm256_sub_epi16(p_lo, vc_lo));
                __m256i pc_hi = _mm256_abs_epi16(_mm256_sub_epi16(p_hi, vc_hi));


                __m256i pa_le_pb_lo = _mm256_xor_si256(_mm256_cmpgt_epi16(pa_lo, pb_lo), all1);
                __m256i pa_le_pb_hi = _mm256_xor_si256(_mm256_cmpgt_epi16(pa_hi, pb_hi), all1);

                __m256i pa_le_pc_lo = _mm256_xor_si256(_mm256_cmpgt_epi16(pa_lo, pc_lo), all1);
                __m256i pa_le_pc_hi = _mm256_xor_si256(_mm256_cmpgt_epi16(pa_hi, pc_hi), all1);

                __m256i pb_le_pc_lo = _mm256_xor_si256(_mm256_cmpgt_epi16(pb_lo, pc_lo), all1);
                __m256i pb_le_pc_hi = _mm256_xor_si256(_mm256_cmpgt_epi16(pb_hi, pc_hi), all1);

                __m256i d_lo = _mm256_blendv_epi8(vb_lo, vc_lo, pb_le_pc_lo);
                __m256i d_hi = _mm256_blendv_epi8(vb_hi, vc_hi, pb_le_pc_hi);

                d_lo = _mm256_blendv_epi8(va_lo, d_lo, _mm256_and_si256(pa_le_pb_lo, pa_le_pc_lo));
                d_hi = _mm256_blendv_epi8(va_hi, d_hi, _mm256_and_si256(pa_le_pb_hi, pa_le_pc_hi));

                __m256i d = _mm256_packus_epi16(d_lo, d_hi);


                __m256i tavg_lo = _mm256_srli_epi16(_mm256_add_epi16(va_lo, vb_lo), 1);
                __m256i tavg_hi = _mm256_srli_epi16(_mm256_add_epi16(va_hi, vb_hi), 1);
                __m256i tavg = _mm256_packus_epi16(tavg_lo, tavg_hi);


                __m256i vsub = _mm256_sub_epi8(r, va);
                __m256i vup = _mm256_sub_epi8(r, vb);
                __m256i vavg = _mm256_sub_epi8(r, tavg);
                __m256i vpaeth = _mm256_sub_epi8(r, d);


                noneSum = _mm256_add_epi64(noneSum, _mm256_sad_epu8(r, zero));
                subSum = _mm256_add_epi64(subSum, _mm256_sad_epu8(r, va));
                upSum = _mm256_add_epi64(upSum, _mm256_sad_epu8(r, vb));
                avgSum = _mm256_add_epi64(avgSum, _mm256_sad_epu8(r, tavg));
                paethSum = _mm256_add_epi64(paethSum, _mm256_sad_epu8(r, d));


                _mm256_storeu_si256((__m256i *)(filter_buffers[0] + j + 1), r);
                _mm256_storeu_si256((__m256i *)(filter_buffers[1] + j + 1), vsub);
                _mm256_storeu_si256((__m256i *)(filter_buffers[2] + j + 1), vup);
                _mm256_storeu_si256((__m256i *)(filter_buffers[3] + j + 1), vavg);
                _mm256_storeu_si256((__m256i *)(filter_buffers[4] + j + 1), vpaeth);
            }

            alignas(32) uint64_t tmp0[4];
            alignas(32) uint64_t tmp1[4];
            alignas(32) uint64_t tmp2[4];
            alignas(32) uint64_t tmp3[4];
            alignas(32) uint64_t tmp4[4];

            _mm256_store_si256((__m256i *)tmp0, noneSum);
            _mm256_store_si256((__m256i *)tmp1, subSum);
            _mm256_store_si256((__m256i *)tmp2, upSum);
            _mm256_store_si256((__m256i *)tmp3, avgSum);
            _mm256_store_si256((__m256i *)tmp4, paethSum);

            for (uint64_t u = 0; u < 4; u++) filter_scores[0] += tmp0[u];
            for (uint64_t u = 0; u < 4; u++) filter_scores[1] += tmp1[u];
            for (uint64_t u = 0; u < 4; u++) filter_scores[2] += tmp2[u];
            for (uint64_t u = 0; u < 4; u++) filter_scores[3] += tmp3[u];
            for (uint64_t u = 0; u < 4; u++) filter_scores[4] += tmp4[u];
            #endif

            for (; j < bpr; j++)
            {
                int p = raw[j - bpp] + raw[j - bpr] - raw[j - bpr - bpp];
                int pa = abs(p - raw[j - bpp]);
                int pb = abs(p - raw[j - bpr]);
                int pc = abs(p - raw[j - bpr - bpp]);

                uint8_t d = (pb <= pc) ? (raw[j - bpr]) : (raw[j - bpr - bpp]);
                d = (pa <= pb && pa <= pc) ? (raw[j - bpp]) : (d);

                filter_buffers[0][j+1] = raw[j];
                filter_buffers[1][j+1] = raw[j] - raw[j - bpp];
                filter_buffers[2][j+1] = raw[j] - raw[j - bpr];
                filter_buffers[3][j+1] = raw[j] - ((raw[j - bpp] + raw[j - bpr]) >> 1);
                filter_buffers[4][j+1] = raw[j] - d;

                filter_scores[0] += abs(filter_buffers[0][j+1]);
                filter_scores[1] += abs(filter_buffers[1][j+1]);
                filter_scores[2] += abs(filter_buffers[2][j+1]);
                filter_scores[3] += abs(filter_buffers[3][j+1]);
                filter_scores[4] += abs(filter_buffers[4][j+1]);
            }
        }

        filter_type = 0;
        for (int i = 0; i < 5; i++) filter_type = (filter_scores[i] < filter_scores[filter_type]) ? (i) : (filter_type);

        strm.next_in = (uint8_t*)filter_buffers[filter_type];
        strm.avail_in = bpr + 1;
        do {
            strm.next_out = out + 8 + have;
            //clock_t start, end;
            //start = clock();
            ret = zng_deflate(&strm, Z_NO_FLUSH);
            //end = clock();
            //deflate_runtime += (double)(end - start) / CLOCKS_PER_SEC;
            if (__builtin_expect(ret != Z_OK, 0)) {
                return_code = 3;
                zng_deflateEnd(&strm);
                goto cleanup;
            }
            have = CHUNK - strm.avail_out;
            if (strm.avail_out == 0) {
                data_len = (uint32_t)(have);
                data_len = edian_swap_u32(data_len, is_little_edian);
                memcpy(out, &data_len, 4);
                uint32_t crc_ = zng_crc32(0, Z_NULL, 0);
                crc_ = zng_crc32(crc_, out + 4, 4);
                crc_ = zng_crc32(crc_, out + 8, have);
                crc_ = edian_swap_u32(crc_, is_little_edian);
                memcpy(out + 8 + have, &crc_, 4);

                if (__builtin_expect(fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4, 0)) {
                    return_code = 1;
                    zng_deflateEnd(&strm);
                    goto cleanup;
                }

                strm.avail_out = CHUNK;
                have = 0;
            }
        } while (strm.avail_in > 0);
    }

    do {
        strm.next_out = out + 8 + have;
        //clock_t start, end;
        //start = clock();
        ret = zng_deflate(&strm, Z_FINISH);
        //end = clock();
        //deflate_runtime += (double)(end - start) / CLOCKS_PER_SEC;
        if (__builtin_expect(ret != Z_OK && ret != Z_STREAM_END, 0)) {
            return_code = 3;
            zng_deflateEnd(&strm);
            goto cleanup;
        }
        have = CHUNK - strm.avail_out;
        if (strm.avail_out == 0) {
            data_len = (uint32_t)(have);
            data_len = edian_swap_u32(data_len, is_little_edian);
            memcpy(out, &data_len, 4);
            uint32_t crc_ = zng_crc32(0, Z_NULL, 0);
            crc_ = zng_crc32(crc_, out + 4, 4 + have);
            crc_ = edian_swap_u32(crc_, is_little_edian);
            memcpy(out + 8 + have, &crc_, 4);
            if (__builtin_expect(fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4, 0)) {
                return_code = 1;
                zng_deflateEnd(&strm);
                goto cleanup;
            }
            strm.avail_out = CHUNK;
            have = 0;
        }
    } while (ret != Z_STREAM_END);

    data_len = (uint32_t)(have);
    data_len = edian_swap_u32(data_len, is_little_edian);
    memcpy(out, &data_len, 4);
    uint32_t crc_ = zng_crc32(0, Z_NULL, 0);
    crc_ = zng_crc32(crc_, out + 4, 4 + have);
    crc_ = edian_swap_u32(crc_, is_little_edian);
    memcpy(out + 8 + have, &crc_, 4);
    if (__builtin_expect(fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4, 0)) {
        return_code = 1;
        zng_deflateEnd(&strm);
        goto cleanup;
    }
    zng_deflateEnd(&strm);
    // finish writting IDAT














    // CHUNK AFTER IDAT STAY HERE















    // writting IEND
    if (__builtin_expect(fwrite(IENDsig, 1, 12, file) != 12, 0)) {
        return_code = 1;
        goto cleanup;
    }
    // finish IEND
    //printf("deflate: %.6fs\n", deflate_runtime);
cleanup:
    free(out);
    for (int i = 0; i < 5; i++) free(filter_buffers[i]);
    return return_code;
}




// x must >= 0
static inline uint64_t ceil__(double x) {
    uint64_t a = (uint64_t)x;
    return a + (x > a);
}

