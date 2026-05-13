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
#include <slp_png_read.h>
#include <slp_image.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
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
#define __bswap_constant_64(x)              \
   ((((x) & 0xff00000000000000ull) >> 56) | \
    (((x) & 0x00ff000000000000ull) >> 40) | \
    (((x) & 0x0000ff0000000000ull) >> 24) | \
    (((x) & 0x000000ff00000000ull) >> 8)  | \
    (((x) & 0x00000000ff000000ull) << 8)  | \
    (((x) & 0x0000000000ff0000ull) << 24) | \
    (((x) & 0x000000000000ff00ull) << 40) | \
    (((x) & 0x00000000000000ffull) << 56))

#define edian_swap_u32(x, is_little_edian) ((is_little_edian) ? (__bswap_constant_32(x)) : (x))
#define edian_swap_u64(x, is_little_edian) ((is_little_edian) ? (__bswap_constant_64(x)) : (x))


// helper
// functions
static int slp_png_get_channels(int color_type, int bit_depth);


static int slp_png_defilter(uint8_t *buffer, uint8_t* scanline[2], const uint64_t bpp, const uint64_t bpr, const uint64_t imtrker); // defilter engine, using scanline[0] as the up scanline and scanline[1] as the stream scanline each time

//
static void slp_png_decode(struct slp_image *slp_png_stream, FILE *file, uint64_t file_size);

//
static void slp_png_colortype3_decode(struct slp_image *slp_png_stream, FILE *file, uint64_t file_size);


static void slp_png_colortype3_unpack(uint8_t* buffer, struct slp_image *slp_png_stream, const uint64_t bpr, const uint64_t imtrker);

static uint64_t ceil__(double x);





// constants
enum {
    PNGsig  = 0x89504E470D0A1A0A,
    IHDRsig = 0x49484452,
    IDATsig = 0x49444154,
    IENDsig = 0x49454E44,
    PLTEsig = 0x504C5445,
    tRNSsig = 0x74524E53
};
/*
const unsigned char PNGsig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
const unsigned char IHDRsig[4] = {'I', 'H', 'D', 'R'};
const unsigned char IDATsig[4] = {'I', 'D', 'A', 'T'};
const unsigned char IENDsig[4] = {'I', 'E', 'N', 'D'};
const unsigned char PLTEsig[4] = {'P', 'L', 'T', 'E'};
const unsigned char tRNSsig[4] = {'t', 'R', 'N', 'S'};
*/
const uint64_t CHUNK = 65536;









// read png from file
struct slp_image slp_png_read(const char path[]) {

    uint16_t check_if_little_edian_temp_value = 1;
    const bool is_little_edian = (*(uint8_t*)(&check_if_little_edian_temp_value));

    struct slp_image slp_png_stream = {0};
    FILE* file;

    file = fopen(path, "rb");
    if (file == NULL) {
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    int ret = fseek(file, 0, SEEK_END);
    if (__builtin_expect(ret != 0, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    long file_size = ftell(file);
    if (__builtin_expect(file_size < 57, 0)) {// minimal size required for PNGSIG + IHDR + IDAT(with data len = 0) + IEND
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    ret = fseek(file, 0, SEEK_SET);
    if (__builtin_expect(ret != 0, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    uint8_t worker[33];

    if (__builtin_expect(fread(worker, 1, 33, file) < 33, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    if (__builtin_expect(edian_swap_u64(*(uint64_t*)(worker), is_little_edian) != PNGsig && edian_swap_u32(*(uint32_t*)(worker + 8), is_little_edian) != 13 && edian_swap_u32(*(uint32_t*)(worker + 12), is_little_edian) != IHDRsig, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    uint32_t crc_ = zng_crc32(0U, Z_NULL, 0);
    crc_ = zng_crc32(crc_, worker + 12, 4);
    crc_ = zng_crc32(crc_, worker + 16, 13);

    if (__builtin_expect(edian_swap_u32(*(uint32_t*)(worker + 29), is_little_edian) != crc_, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    const uint32_t width = slp_png_stream.width = edian_swap_u32(*(uint32_t*)(worker + 16), is_little_edian);
    const uint32_t height = slp_png_stream.height = edian_swap_u32(*(uint32_t*)(worker + 20), is_little_edian);
    const int bit_depth = slp_png_stream.bit_depth = worker[24];
    const int color_type = worker[25];
    const int channels = slp_png_stream.channels = slp_png_get_channels(color_type, bit_depth);
    const int compression_method = worker[26];
    const int filter_method = worker[27];
    const int interlace_method = worker[28];


    if (__builtin_expect((compression_method | filter_method | interlace_method) != 0 | channels == 0, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    const uint64_t size = (uint64_t)width * (uint64_t)height * (uint64_t)channels * (uint64_t)(1 + (bit_depth == 16));

    slp_png_stream.buffer = (uint8_t*)malloc(size);

    if (__builtin_expect(slp_png_stream.buffer == NULL, 0)) {
        fclose(file);
        slp_png_stream.bit_depth = -1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    if (color_type != 3) {
        slp_png_decode(&slp_png_stream, file, file_size);

        if (__builtin_expect(slp_png_stream.bit_depth != bit_depth, 0)) {
            fclose(file);
            free(slp_png_stream.buffer);
            slp_png_stream.buffer = NULL;
            return slp_png_stream;
        }
    }
    else {
        slp_png_colortype3_decode(&slp_png_stream, file, file_size);

        if (__builtin_expect(slp_png_stream.bit_depth != bit_depth, 0)) {
            fclose(file);
            free(slp_png_stream.buffer);
            slp_png_stream.buffer = NULL;
            return slp_png_stream;
        }
        slp_png_stream.bit_depth = 8;
    }





    // EXTRA FORMATTING HERE




    fclose(file);
    return slp_png_stream;
}










static inline int slp_png_get_channels(int color_type, int bit_depth) {
    int channels;
    switch (color_type) {
        case 0: {
            channels = 1;
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        case 2: {
            channels = 3;
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        case 3: {
            channels = 4;
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                default: return 0;
            }
            break;
        }
        case 4: {
            channels = 2;
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        case 6: {
            channels = 4;
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        default: return 0;
    }
    return channels;
}


//
static inline void slp_png_decode(struct slp_image *slp_png_stream, FILE *file, uint64_t file_size) {

    uint16_t check_if_little_edian_temp_value = 1;
    const bool is_little_edian = (*(uint8_t*)(&check_if_little_edian_temp_value));
    
    uint8_t worker[12];

    uint64_t data_len = 0;
    int idat_check = 0;
    int iend_check = 0;

    const uint64_t bpp = slp_png_stream->channels * (slp_png_stream->bit_depth == 16 ? 2 : 1);
    const uint64_t bpr = ceil__(((double)slp_png_stream->width * slp_png_stream->channels * slp_png_stream->bit_depth) / 8.0);

    //
    do {
        if (__builtin_expect(fread(worker, 1, 8, file) != 8, 0)) {
            slp_png_stream->bit_depth = 1;
            goto cleanup;
        }

        uint32_t chunk_type = edian_swap_u32(*(uint32_t*)(worker + 4), is_little_edian);
        data_len = edian_swap_u32(*(uint32_t*)worker, is_little_edian);

        //
        switch (chunk_type) {




            // ADD MORE CASE HERE






            // isIDAT
            case IDATsig: {
                idat_check++;
                if (__builtin_expect(idat_check > 1, 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }


                zng_stream strm = {0};
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                strm.avail_in = 0;
                strm.next_in = Z_NULL;
                int ret = zng_inflateInit(&strm);
                if (__builtin_expect(ret != Z_OK, 0)) {
                    slp_png_stream->bit_depth = 3;
                    goto cleanup;
                }


                uint64_t imtrker = 0;
                uint64_t ai = CHUNK;
                uint64_t ftrker = 0;
                uint64_t intrker = 0;
                uint64_t offset = 0;
                uint64_t have = 0;
                uint64_t row_produced = 0;
                uint32_t crc = 0;
                uint8_t out[CHUNK];
                uint8_t in[CHUNK];


                uint8_t* scanline[2];
                scanline[0] = slp_png_stream->buffer;
                scanline[1] = slp_png_stream->buffer;

                // data_len, ++12, data_len,...
                do {
                    data_len = edian_swap_u32(*(uint32_t*)worker, is_little_edian);
                    if (__builtin_expect(file_size <= data_len, 0)) {
                        slp_png_stream->bit_depth = 1;
                        zng_deflateEnd(&strm);
                        goto cleanup;
                    }

                    crc = zng_crc32(0U, Z_NULL, 0);
                    crc = zng_crc32(crc, worker + 4, 4);

                    if (data_len < ai) {
                        if (__builtin_expect(fread(in + intrker, 1, data_len, file) != data_len, 0)) {
                            slp_png_stream->bit_depth = 1;
                            zng_deflateEnd(&strm);
                            goto cleanup;
                        }
                        ai -= data_len;
                        crc = zng_crc32(crc, in + intrker, data_len);
                        intrker += data_len;
                    }
                    else {
                        if (__builtin_expect(fread(in + intrker, 1, ai, file) != ai, 0)) {
                            slp_png_stream->bit_depth = 1;
                            zng_deflateEnd(&strm);
                            goto cleanup;
                        }
                        crc = zng_crc32(crc, in + intrker, ai);
                        ftrker = data_len - ai;
                        intrker += ai;
                        ai = 0;
                        strm.avail_in = intrker;
                        strm.next_in = in;
                        do {
                            do {
                                strm.avail_out = CHUNK - offset;
                                strm.next_out = out + offset;
                                ret = zng_inflate(&strm, Z_NO_FLUSH);
                                have = CHUNK - strm.avail_out;
                                if (__builtin_expect(ret != Z_OK && ret != Z_STREAM_END, 0)) {
                                    slp_png_stream->bit_depth = 3;
                                    zng_deflateEnd(&strm);
                                    goto cleanup;
                                }
                                row_produced = (have / (bpr + 1));
                                offset = (have - row_produced * (bpr + 1));
                                for (uint64_t i = 0; i < row_produced; i++) {

                                    // defilter to scanline[1] from buffer and scanline[0]
                                    if (__builtin_expect(slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0, 0)) {
                                        slp_png_stream->bit_depth = 2;
                                        zng_deflateEnd(&strm);
                                        goto cleanup;
                                    }

                                    // move scanline for the next process
                                    scanline[0] = scanline[1];
                                    scanline[1] += bpr;

                                    imtrker++;
                                }
                                memmove(out, out + have - offset, offset);
                            } while (strm.avail_in > 0);
                            ai = CHUNK;
                            intrker = 0;
                            if (ftrker > ai) {
                                if (__builtin_expect(fread(in + intrker, 1, ai, file) != ai, 0)) {
                                    slp_png_stream->bit_depth = 1;
                                    zng_deflateEnd(&strm);
                                    goto cleanup;
                                }
                                crc = zng_crc32(crc, in + intrker, ai);
                                ftrker -= ai;
                                intrker += ai;
                                strm.avail_in = intrker;
                                strm.next_in = in;
                                ai = 0;
                            }
                            else {
                                if (__builtin_expect(fread(in + intrker, 1, ftrker, file) != ftrker, 0)) {
                                    slp_png_stream->bit_depth = 1;
                                    zng_deflateEnd(&strm);
                                    goto cleanup;
                                }
                                crc = zng_crc32(crc, in + intrker, ftrker);
                                intrker += ftrker;
                                ai -= (ftrker);
                                ftrker = 0;
                            }
                        } while (ftrker != 0);
                    }

                    if (__builtin_expect(fread(worker + 8, 1, 4, file) != 4, 0)) {
                        slp_png_stream->bit_depth = 1;
                        zng_deflateEnd(&strm);
                        goto cleanup;
                    }

                    if (edian_swap_u32(*(uint32_t*)(worker + 8), is_little_edian) != crc) {
                        slp_png_stream->bit_depth = 2;
                        zng_deflateEnd(&strm);
                        goto cleanup;
                    }

                    if (__builtin_expect(fread(worker, 1, 8, file) != 8, 0)) {
                        slp_png_stream->bit_depth = 1;
                        zng_deflateEnd(&strm);
                        goto cleanup;
                    }
                } while (edian_swap_u32(*(uint32_t*)(worker + 4), is_little_edian) == IDATsig);
                strm.avail_in = intrker;
                strm.next_in = in;
                do {
                    strm.avail_out = CHUNK - offset;
                    strm.next_out = out + offset;
                    ret = zng_inflate(&strm, Z_NO_FLUSH);
                    have = CHUNK - strm.avail_out;
                    if (__builtin_expect(ret != Z_OK && ret != Z_STREAM_END, 0)) {
                        slp_png_stream->bit_depth = 3;
                        goto cleanup;
                    }
                    row_produced = (have / (bpr + 1));
                    offset = have - row_produced * (bpr + 1);
                    for (uint64_t i = 0; i < row_produced; i++) {

                        // defilter to scanline[1] from buffer and scanline[0]
                        if (__builtin_expect(slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0, 0)) {
                            slp_png_stream->bit_depth = 2;
                            goto cleanup;
                        }

                        // move scanline for the next process
                        scanline[0] = scanline[1];
                        scanline[1] += bpr;

                        imtrker++;
                    }
                    memmove(out, out + have - offset, offset);
                } while (ret != Z_STREAM_END);
                // if (offset != 0) throw std::runtime_error("data loss");
                zng_inflateEnd(&strm);
                if (__builtin_expect(fseek(file, -8, SEEK_CUR) != 0, 0)) {
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                break;
            }

            // isIEND
            case IENDsig: {
                if (__builtin_expect(idat_check == 0, 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }
                iend_check = 1;
                break;
            }

            // else = skip
            default: {
                fseek(file, data_len + 4, SEEK_CUR);
                break;
            }
        }

    } while ((uint64_t)(ftell(file)) <= (file_size - 12) && iend_check == 0);

    if (iend_check == 0) {
        slp_png_stream->bit_depth = 2;
        goto cleanup;
    }

cleanup:
    return;
}



static inline int slp_png_defilter(uint8_t *buffer, uint8_t* scanline[2], const uint64_t bpp, const uint64_t bpr, const uint64_t imtrker) {
    uint8_t filter = *buffer++;
    switch (filter) {
        case 0: {
            memcpy(scanline[1], buffer, bpr);
            break;
        }
        case 1: {
            memcpy(scanline[1], buffer, bpp);
            for (uint64_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[1][i - bpp];
            break;
        }
        case 2: {
            if (__builtin_expect(imtrker == 0, 0)) memcpy(scanline[1], buffer, bpr);
            else {
                uint64_t i = 0;
                #ifdef __AVX2__
                for (; i + 32 <= bpr; i += 32) {
                    __m256i raw = _mm256_loadu_si256((const __m256i *)(buffer + i));
                    __m256i up  = _mm256_loadu_si256((const __m256i *)(scanline[0] + i));
                    _mm256_storeu_si256((__m256i *)(scanline[1] + i), _mm256_add_epi8(raw, up));
                }
                #endif
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[0][i];
            }
            break;
        }
        case 3: {
            if (__builtin_expect(imtrker == 0, 0)) {
                memcpy(scanline[1], buffer, bpp);
                for (uint64_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + (scanline[1][i - bpp] >> 1);
            }
            else {
                uint64_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + ((scanline[0][i]) >> 1);
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + ((scanline[0][i] + scanline[1][i - bpp]) >> 1);
            }
            break;
        }
        case 4: {
            if (__builtin_expect(imtrker == 0, 0)) {
                memcpy(scanline[1], buffer, bpp);
                for (uint64_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[1][i - bpp];
            }
            else {
                uint64_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + scanline[0][i];
                for (; i < bpr; i++) {
                    int p = scanline[1][i - bpp] + scanline[0][i] - scanline[0][i - bpp];
                    int pa = abs(p - scanline[1][i - bpp]);
                    int pb = abs(p - scanline[0][i]);
                    int pc = abs(p - scanline[0][i - bpp]);

                    uint8_t d = (pb <= pc) ? (scanline[0][i]) : (scanline[0][i - bpp]);
                    d = (pa <= pb && pa <= pc) ? (scanline[1][i - bpp]) : (d);

                    scanline[1][i] = buffer[i] + d;
                }
            }
            break;
        }
        default: return 1;
    }

    return 0;
}


//
static inline void slp_png_colortype3_decode(struct slp_image *slp_png_stream, FILE *file, uint64_t file_size) {

    uint16_t check_if_little_edian_temp_value = 1;
    const bool is_little_edian = (*(uint8_t*)(&check_if_little_edian_temp_value));

    uint8_t worker[12];

    uint64_t data_len = 0;
    int plte_check = 0;
    int idat_check = 0;
    int tRNS_check = 0;
    int iend_check = 0;

    uint8_t* palette = NULL;
    uint64_t entries = 0;

    const uint64_t bpp = 1;
    const uint64_t bpr = ceil__(((double)slp_png_stream->width * (double)slp_png_stream->bit_depth) / (double)8);

    //
    do {
        if (__builtin_expect(fread(worker, 1, 8, file) != 8, 0)) {
            slp_png_stream->bit_depth = 1;
            goto cleanup;
        }

        uint32_t chunk_type = edian_swap_u32(*(uint32_t*)(worker + 4), is_little_edian);
        data_len = edian_swap_u32(*(uint32_t*)worker, is_little_edian);

        //
        switch (chunk_type) {



            // ADD MORE CASE HERE




            // isIDAT
            case IDATsig: {

                idat_check++;
                if (__builtin_expect(idat_check > 1, 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }


                zng_stream strm = {0};
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                strm.avail_in = 0;
                strm.next_in = Z_NULL;
                int ret = zng_inflateInit(&strm);
                if (__builtin_expect(ret != Z_OK, 0)) {
                    slp_png_stream->bit_depth = 3;
                    goto cleanup;
                }


                uint64_t imtrker = 0;
                uint64_t ai = CHUNK;
                uint64_t ftrker = 0;
                uint64_t intrker = 0;
                uint64_t offset = 0;
                uint64_t have = 0;
                uint64_t row_produced = 0;
                uint32_t crc = 0;
                uint8_t out[CHUNK];
                uint8_t in[CHUNK];


                uint8_t* scanline[2];
                scanline[0] = (uint8_t*)malloc(bpr);
                scanline[1] = (uint8_t*)malloc(bpr);
                if (__builtin_expect(scanline[0] == NULL || scanline[1] == NULL, 0)) {
                    slp_png_stream->bit_depth = -1;
                    goto cleanup;
                }
                
                // data_len, ++12, data_len,...
                do {
                    data_len = edian_swap_u32(*(uint32_t*)worker, is_little_edian);
                    if (__builtin_expect(file_size <= data_len, 0)) {
                        free(scanline[0]);
                        free(scanline[1]);
                        slp_png_stream->bit_depth = 1;
                        goto cleanup;
                    }

                    crc = zng_crc32(0U, Z_NULL, 0);
                    crc = zng_crc32(crc, worker + 4, 4);

                    if (data_len < ai) {
                        if (__builtin_expect(fread(in + intrker, 1, data_len, file) != data_len, 0)) {
                            free(scanline[0]);
                            free(scanline[1]);
                            slp_png_stream->bit_depth = 1;
                            goto cleanup;
                        }
                        ai -= data_len;
                        crc = zng_crc32(crc, in + intrker, data_len);
                        intrker += data_len;
                    }
                    else {
                        if (__builtin_expect(fread(in + intrker, 1, ai, file) != ai, 0)) {
                            free(scanline[0]);
                            free(scanline[1]);
                            slp_png_stream->bit_depth = 1;
                            goto cleanup;
                        }
                        crc = zng_crc32(crc, in + intrker, ai);
                        ftrker = data_len - ai;
                        intrker += ai;
                        ai = 0;
                        strm.avail_in = intrker;
                        strm.next_in = in;
                        do {
                            do {
                                strm.avail_out = CHUNK - offset;
                                strm.next_out = out + offset;
                                ret = zng_inflate(&strm, Z_NO_FLUSH);
                                have = CHUNK - strm.avail_out;
                                if (__builtin_expect(ret != Z_OK, 0)) {
                                    free(scanline[0]);
                                    free(scanline[1]);
                                    slp_png_stream->bit_depth = 3;
                                    goto cleanup;
                                }
                                row_produced = (have / (bpr + 1));
                                offset = (have - row_produced * (bpr + 1));
                                for (uint64_t i = 0; i < row_produced; i++) {

                                    // defilter to scanline[1] from buffer and scanline[0]
                                    if (__builtin_expect(slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0, 0)) {
                                        free(scanline[0]);
                                        free(scanline[1]);
                                        slp_png_stream->bit_depth = 2;
                                        goto cleanup;
                                    }

                                    slp_png_colortype3_unpack(scanline[1], slp_png_stream, bpr, imtrker);

                                    // swap scanline for the next process
                                    uint8_t* temp = scanline[0];
                                    scanline[0] = scanline[1];
                                    scanline[1] = temp;

                                    imtrker++;
                                }
                                memmove(out, out + have - offset, offset);
                            } while (strm.avail_in > 0);
                            ai = CHUNK;
                            intrker = 0;
                            if (ftrker > ai) {
                                if (__builtin_expect(fread(in + intrker, 1, ai, file) != ai, 0)) {
                                    free(scanline[0]);
                                    free(scanline[1]);
                                    slp_png_stream->bit_depth = 1;
                                    goto cleanup;
                                }
                                crc = zng_crc32(crc, in + intrker, ai);
                                ftrker -= ai;
                                intrker += ai;
                                strm.avail_in = intrker;
                                strm.next_in = in;
                                ai = 0;
                            }
                            else {
                                if (__builtin_expect(fread(in + intrker, 1, ftrker, file) != ftrker, 0)) {
                                    free(scanline[0]);
                                    free(scanline[1]);
                                    slp_png_stream->bit_depth = 1;
                                    goto cleanup;
                                }
                                crc = zng_crc32(crc, in + intrker, ftrker);
                                intrker += ftrker;
                                ai -= (ftrker);
                                ftrker = 0;
                            }
                        } while (ftrker != 0);
                    }

                    if (__builtin_expect(fread(worker + 8, 1, 4, file) != 4, 0)) {
                        free(scanline[0]);
                        free(scanline[1]);
                        slp_png_stream->bit_depth = 1;
                        goto cleanup;
                    }

                    if (edian_swap_u32(*(uint32_t*)(worker + 8), is_little_edian) != crc) {
                        free(scanline[0]);
                        free(scanline[1]);
                        slp_png_stream->bit_depth = 2;
                        goto cleanup;
                    }

                    if (__builtin_expect(fread(worker, 1, 8, file) != 8, 0)) {
                        free(scanline[0]);
                        free(scanline[1]);
                        slp_png_stream->bit_depth = 1;
                        goto cleanup;
                    }
                } while (edian_swap_u32(*(uint32_t*)(worker + 4), is_little_edian) == IDATsig);

                strm.avail_in = intrker;
                strm.next_in = in;
                do {
                    strm.avail_out = CHUNK - offset;
                    strm.next_out = out + offset;
                    ret = zng_inflate(&strm, Z_NO_FLUSH);
                    have = CHUNK - strm.avail_out;

                    if (__builtin_expect(ret != Z_OK && ret != Z_STREAM_END, 0)) {
                        free(scanline[0]);
                        free(scanline[1]);
                        slp_png_stream->bit_depth = 3;
                        goto cleanup;
                    }
                    row_produced = (have / (bpr + 1));
                    offset = have - row_produced * (bpr + 1);
                    for (uint64_t i = 0; i < row_produced; i++) {

                        // defilter to scanline[1] from buffer and scanline[0]
                        if (__builtin_expect(slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0, 0)) {
                            free(scanline[0]);
                            free(scanline[1]);
                            slp_png_stream->bit_depth = 2;
                            goto cleanup;
                        }

                        slp_png_colortype3_unpack(scanline[1], slp_png_stream, bpr, imtrker);

                        // swap scanline for the next process
                        uint8_t* temp = scanline[0];
                        scanline[0] = scanline[1];
                        scanline[1] = temp;

                        imtrker++;
                    }
                    memmove(out, out + have - offset, offset);
                } while (ret != Z_STREAM_END);

                // if (offset != 0) throw std::runtime_error("data loss");
                zng_inflateEnd(&strm);
                if (__builtin_expect(fseek(file, -8, SEEK_CUR) != 0, 0)) {
                    free(scanline[0]);
                    free(scanline[1]);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                free(scanline[0]);
                free(scanline[1]);

                break;
            }

            // PLTE
            case PLTEsig: {
                plte_check++;
                if (__builtin_expect(plte_check > 1, 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                uint8_t* plte = (uint8_t*)malloc(data_len);
                if (__builtin_expect(plte == NULL, 0)) {
                    slp_png_stream->bit_depth = -1;
                    goto cleanup;
                }

                if (__builtin_expect(fread(plte, 1, data_len, file) != data_len, 0)) {
                    free(plte);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                uint32_t crc_ = zng_crc32(0U, Z_NULL, 0);
                crc_ = zng_crc32(crc_, worker + 4, 4);
                crc_ = zng_crc32(crc_, plte, data_len);


                if (__builtin_expect(fread(worker + 8, 1, 4, file) != 4, 0)) {
                    free(plte);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                if (__builtin_expect(plte_check > 1 || (data_len) % 3 != 0 || !(data_len / 3 >= 0 && data_len / 3 <= 256) || edian_swap_u32(*(uint32_t*)(worker + 8), is_little_edian) != crc_, 0)) {
                    free(plte);
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                entries = data_len / 3;
                palette = (uint8_t*)malloc(entries * 4);
                if (__builtin_expect(palette == NULL, 0)) {
                    free(plte);
                    slp_png_stream->bit_depth = -1;
                    goto cleanup;
                }

                uint64_t k = 0;
                for (uint64_t i = 0; i < entries; i++) {
                    palette[i * 4 + 0] = plte[k++];
                    palette[i * 4 + 1] = plte[k++];
                    palette[i * 4 + 2] = plte[k++];
                    palette[i * 4 + 3] = 255;
                }

                free(plte);

                break;
            }

            // tRNS
            case tRNSsig: {
                tRNS_check++;
                if (__builtin_expect(tRNS_check > 1 || plte_check == 0 || plte_check == 0 || data_len > entries, 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                uint8_t* trns = (uint8_t*)malloc(data_len);
                if (__builtin_expect(trns == NULL, 0)) {
                    slp_png_stream->bit_depth = -1;
                    goto cleanup;
                }

                if (__builtin_expect(fread(trns, 1, data_len, file) != data_len, 0)) {
                    free(trns);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                uint32_t crc_ = zng_crc32(0U, Z_NULL, 0);
                crc_ = zng_crc32(crc_, worker + 4, 4);
                crc_ = zng_crc32(crc_, trns, data_len);

                if (__builtin_expect(fread(worker + 8, 1, 4, file) != 4, 0)) {
                    free(trns);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                if (edian_swap_u32(*(uint32_t*)(worker + 8), is_little_edian) != crc_) {
                    free(trns);
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                for (uint64_t i = 0; i < data_len; i++) palette[i * 4 + 3] = trns[i];

                free(trns);

                break;
            }

            // isIEND
            case IENDsig: {
                if (__builtin_expect(idat_check == 0 || plte_check == 0, 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }
                iend_check = 1;
                break;
            }

            // else = skip
            default: {
                fseek(file, data_len + 4, SEEK_CUR);
                break;
            }
        }

    } while ((uint64_t)(ftell(file)) <= (file_size - 12) && iend_check == 0);

    if (__builtin_expect(iend_check == 0, 0)) {
        slp_png_stream->bit_depth = 2;
        goto cleanup;
    }

    uint8_t *im = slp_png_stream->buffer;

    for (uint64_t i = 0; i < (uint64_t)(slp_png_stream->height) * (uint64_t)(slp_png_stream->width); i++) {
        uint8_t index = *im;
        *im++ = palette[index * 4 + 0];
        *im++ = palette[index * 4 + 1];
        *im++ = palette[index * 4 + 2];
        *im++ = palette[index * 4 + 3];
    }

cleanup: 
    free(palette);
    return;
}



static inline void slp_png_colortype3_unpack(uint8_t* buffer, struct slp_image *slp_png_stream, const uint64_t bpr, const uint64_t imtrker) {
    
    uint8_t *src = buffer;
    uint8_t *dest = slp_png_stream->buffer + imtrker * (uint64_t)(slp_png_stream->width) * (uint64_t)(slp_png_stream->channels);

    switch (slp_png_stream->bit_depth) {
    case 1: {
        uint64_t i = 0;
        #ifdef __SSE2__
        __m128i ones = _mm_set1_epi16(1);
        __m128i zeroes = _mm_setzero_si128();

        for (; i + 16 <= bpr; i += 16) {
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

            //_mm_storeu_si128((__m128i *)(dest + 0 * 16), c0);
            __m128i c0_lo = _mm_unpacklo_epi8(c0, zeroes);
            __m128i c0_hi = _mm_unpackhi_epi8(c0, zeroes);

            __m128i p0 = _mm_unpacklo_epi16(c0_lo, zeroes);
            __m128i p1 = _mm_unpackhi_epi16(c0_lo, zeroes);
            __m128i p2 = _mm_unpacklo_epi16(c0_hi, zeroes);
            __m128i p3 = _mm_unpackhi_epi16(c0_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 1 * 16), c4);
            __m128i c4_lo = _mm_unpacklo_epi8(c4, zeroes);
            __m128i c4_hi = _mm_unpackhi_epi8(c4, zeroes);

            p0 = _mm_unpacklo_epi16(c4_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c4_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c4_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c4_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 2 * 16), c1);
            __m128i c1_lo = _mm_unpacklo_epi8(c1, zeroes);
            __m128i c1_hi = _mm_unpackhi_epi8(c1, zeroes);

            p0 = _mm_unpacklo_epi16(c1_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c1_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c1_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c1_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 3 * 16), c5);
            __m128i c5_lo = _mm_unpacklo_epi8(c5, zeroes);
            __m128i c5_hi = _mm_unpackhi_epi8(c5, zeroes);

            p0 = _mm_unpacklo_epi16(c5_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c5_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c5_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c5_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 4 * 16), c2);
            __m128i c2_lo = _mm_unpacklo_epi8(c2, zeroes);
            __m128i c2_hi = _mm_unpackhi_epi8(c2, zeroes);

            p0 = _mm_unpacklo_epi16(c2_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c2_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c2_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c2_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 5 * 16), c6);
            __m128i c6_lo = _mm_unpacklo_epi8(c6, zeroes);
            __m128i c6_hi = _mm_unpackhi_epi8(c6, zeroes);

            p0 = _mm_unpacklo_epi16(c6_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c6_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c6_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c6_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 6 * 16), c3);
            __m128i c3_lo = _mm_unpacklo_epi8(c3, zeroes);
            __m128i c3_hi = _mm_unpackhi_epi8(c3, zeroes);

            p0 = _mm_unpacklo_epi16(c3_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c3_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c3_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c3_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 7 * 16), c7);
            __m128i c7_lo = _mm_unpacklo_epi8(c7, zeroes);
            __m128i c7_hi = _mm_unpackhi_epi8(c7, zeroes);

            p0 = _mm_unpacklo_epi16(c7_lo, zeroes);
            p1 = _mm_unpackhi_epi16(c7_lo, zeroes);
            p2 = _mm_unpacklo_epi16(c7_hi, zeroes);
            p3 = _mm_unpackhi_epi16(c7_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;
            src += 16;
        }
        #endif
        for (; i < bpr; i++) {
            for (int j = 7; j >= 0; j--) {
                *dest = ((*src >> j) & 1);
                dest += 4;
            }
            src++;
        }
        break;
    }
    case 2: {
        uint64_t i = 0;
        #ifdef __SSE2__
        __m128i ones = _mm_set1_epi16(0b11);
        __m128i zeroes = _mm_setzero_si128();

        for (; i + 16 <= bpr; i += 16) {
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


            //_mm_storeu_si128((__m128i *)(dest + 0 * 16), b0);
            __m128i b0_lo = _mm_unpacklo_epi8(b0, zeroes);
            __m128i b0_hi = _mm_unpackhi_epi8(b0, zeroes);

            __m128i p0 = _mm_unpacklo_epi16(b0_lo, zeroes);
            __m128i p1 = _mm_unpackhi_epi16(b0_lo, zeroes);
            __m128i p2 = _mm_unpacklo_epi16(b0_hi, zeroes);
            __m128i p3 = _mm_unpackhi_epi16(b0_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 1 * 16), b2);
            __m128i b2_lo = _mm_unpacklo_epi8(b2, zeroes);
            __m128i b2_hi = _mm_unpackhi_epi8(b2, zeroes);

            p0 = _mm_unpacklo_epi16(b2_lo, zeroes);
            p1 = _mm_unpackhi_epi16(b2_lo, zeroes);
            p2 = _mm_unpacklo_epi16(b2_hi, zeroes);
            p3 = _mm_unpackhi_epi16(b2_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 2 * 16), b1);
            __m128i b1_lo = _mm_unpacklo_epi8(b1, zeroes);
            __m128i b1_hi = _mm_unpackhi_epi8(b1, zeroes);

            p0 = _mm_unpacklo_epi16(b1_lo, zeroes);
            p1 = _mm_unpackhi_epi16(b1_lo, zeroes);
            p2 = _mm_unpacklo_epi16(b1_hi, zeroes);
            p3 = _mm_unpackhi_epi16(b1_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 3 * 16), b3);
            __m128i b3_lo = _mm_unpacklo_epi8(b3, zeroes);
            __m128i b3_hi = _mm_unpackhi_epi8(b3, zeroes);

            p0 = _mm_unpacklo_epi16(b3_lo, zeroes);
            p1 = _mm_unpackhi_epi16(b3_lo, zeroes);
            p2 = _mm_unpacklo_epi16(b3_hi, zeroes);
            p3 = _mm_unpackhi_epi16(b3_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;
            src += 16;
        }
        #endif
        for (; i < bpr; i++) {
            for (int j = 3; j >= 0; j--) {
                *dest++ = ((*src >> (j * 2)) & 3);
                dest += 4;
            }
            src++;
        }
        break;
    }
    case 4: {
        uint64_t i = 0;
        #ifdef __SSE2__
        __m128i m0 = _mm_set1_epi8(0b11110000);
        __m128i m1 = _mm_set1_epi8(0b00001111);
        __m128i zeroes = _mm_setzero_si128();

        for (; i + 16 <= bpr; i += 16) {
            __m128i in = _mm_loadu_si128((const __m128i *)src);

            __m128i in0 = _mm_srli_epi64(_mm_and_si128(in, m0), 4);
            __m128i in1 = _mm_and_si128(in, m1);

            __m128i a0 = _mm_unpacklo_epi8(in0, in1); // 0 x 1 lo
            __m128i a1 = _mm_unpackhi_epi8(in0, in1); // 0 x 1 hi

            //_mm_storeu_si128((__m128i *)(dest + 0 * 16), a0);
            __m128i a0_lo = _mm_unpacklo_epi8(a0, zeroes);
            __m128i a0_hi = _mm_unpackhi_epi8(a0, zeroes);

            __m128i p0 = _mm_unpacklo_epi16(a0_lo, zeroes);
            __m128i p1 = _mm_unpackhi_epi16(a0_lo, zeroes);
            __m128i p2 = _mm_unpacklo_epi16(a0_hi, zeroes);
            __m128i p3 = _mm_unpackhi_epi16(a0_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;

            //_mm_storeu_si128((__m128i *)(dest + 1 * 16), a1);
            __m128i a1_lo = _mm_unpacklo_epi8(a1, zeroes);
            __m128i a1_hi = _mm_unpackhi_epi8(a1, zeroes);

            p0 = _mm_unpacklo_epi16(a1_lo, zeroes);
            p1 = _mm_unpackhi_epi16(a1_lo, zeroes);
            p2 = _mm_unpacklo_epi16(a1_hi, zeroes);
            p3 = _mm_unpackhi_epi16(a1_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;
            src += 16;
        }
        #endif
        for (; i < bpr; i++) {
            for (int j = 1; j >= 0; j--) {
                *dest = ((*src >> (j * 4)) & 0b1111);
                dest += 4;
            }
            src++;
        }
        break;
    }
    case 8: {
        uint64_t i = 0;
        #ifdef __SSE2__
        __m128i zeroes = _mm_setzero_si128();

        for (; i + 16 <= bpr; i += 16) {

            __m128i in = _mm_loadu_si128((const __m128i *)src);

            __m128i in_lo = _mm_unpacklo_epi8(in, zeroes);
            __m128i in_hi = _mm_unpackhi_epi8(in, zeroes);

            __m128i p0 = _mm_unpacklo_epi16(in_lo, zeroes);
            __m128i p1 = _mm_unpackhi_epi16(in_lo, zeroes);
            __m128i p2 = _mm_unpacklo_epi16(in_hi, zeroes);
            __m128i p3 = _mm_unpackhi_epi16(in_hi, zeroes);

            _mm_storeu_si128((__m128i *)(dest + 0 * 16), p0);
            _mm_storeu_si128((__m128i *)(dest + 1 * 16), p1);
            _mm_storeu_si128((__m128i *)(dest + 2 * 16), p2);
            _mm_storeu_si128((__m128i *)(dest + 3 * 16), p3);

            dest += 64;
            src += 16;
        }
        #endif
        for (; i < bpr; i++) {
            *dest = *src;
            dest += 4;
            src++;
        }
        break;
    }
    }
}



void slp_image_delete(struct slp_image *image) {
    free(image->buffer);
    memset(image, 0, sizeof(*image));
}


// x must >= 0
static inline uint64_t ceil__(double x) {
    uint64_t a = (uint64_t)x;
    return a + (x > a);
}
