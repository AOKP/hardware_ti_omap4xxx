/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
* @file Encoder_libjpeg.cpp
*
* This file encodes a YUV422I buffer to a jpeg
* TODO(XXX): Need to support formats other than yuv422i
*            Change interface to pre/post-proc algo framework
*
*/

#define LOG_TAG "CameraHAL"

#include "CameraHal.h"
#include "Encoder_libjpeg.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

extern "C" {
    #include "jpeglib.h"
    #include "jerror.h"
}

#define ARRAY_SIZE(array) (sizeof((array)) / sizeof((array)[0]))

namespace android {
struct string_pair {
    const char* string1;
    const char* string2;
};

static string_pair degress_to_exif_lut [] = {
    // degrees, exif_orientation
    {"0",   "1"},
    {"90",  "6"},
    {"180", "3"},
    {"270", "8"},
};
struct libjpeg_destination_mgr : jpeg_destination_mgr {
    libjpeg_destination_mgr(uint8_t* input, int size);

    uint8_t* buf;
    int bufsize;
    size_t jpegsize;
};

static void libjpeg_init_destination (j_compress_ptr cinfo) {
    libjpeg_destination_mgr* dest = (libjpeg_destination_mgr*)cinfo->dest;

    dest->next_output_byte = dest->buf;
    dest->free_in_buffer = dest->bufsize;
    dest->jpegsize = 0;
}

static boolean libjpeg_empty_output_buffer(j_compress_ptr cinfo) {
    libjpeg_destination_mgr* dest = (libjpeg_destination_mgr*)cinfo->dest;

    dest->next_output_byte = dest->buf;
    dest->free_in_buffer = dest->bufsize;
    return TRUE; // ?
}

static void libjpeg_term_destination (j_compress_ptr cinfo) {
    libjpeg_destination_mgr* dest = (libjpeg_destination_mgr*)cinfo->dest;
    dest->jpegsize = dest->bufsize - dest->free_in_buffer;
}

libjpeg_destination_mgr::libjpeg_destination_mgr(uint8_t* input, int size) {
    this->init_destination = libjpeg_init_destination;
    this->empty_output_buffer = libjpeg_empty_output_buffer;
    this->term_destination = libjpeg_term_destination;

    this->buf = input;
    this->bufsize = size;
}

/* private static functions */

static void uyvy_to_yuv(uint8_t* dst, uint32_t* src, int width) {
    if (!dst || !src) {
        return;
    }

    if (width % 2) {
        return; // not supporting odd widths
    }

    // currently, neon routine only supports multiple of 16 width
    if (width % 16) {
        while ((width-=2) >= 0) {
            uint8_t u0 = (src[0] >> 0) & 0xFF;
            uint8_t y0 = (src[0] >> 8) & 0xFF;
            uint8_t v0 = (src[0] >> 16) & 0xFF;
            uint8_t y1 = (src[0] >> 24) & 0xFF;
            dst[0] = y0;
            dst[1] = u0;
            dst[2] = v0;
            dst[3] = y1;
            dst[4] = u0;
            dst[5] = v0;
            dst += 6;
            src++;
        }
    } else {
        int n = width;
        asm volatile (
        "   pld [%[src], %[src_stride], lsl #2]                         \n\t"
        "   cmp %[n], #16                                               \n\t"
        "   blt 5f                                                      \n\t"
        "0: @ 16 pixel swap                                             \n\t"
        "   vld2.8  {q0, q1} , [%[src]]! @ q0 = uv q1 = y               \n\t"
        "   vuzp.8 q0, q2                @ d1 = u d5 = v                \n\t"
        "   vmov d1, d0                  @ q0 = u0u1u2..u0u1u2...       \n\t"
        "   vmov d5, d4                  @ q2 = v0v1v2..v0v1v2...       \n\t"
        "   vzip.8 d0, d1                @ q0 = u0u0u1u1u2u2...         \n\t"
        "   vzip.8 d4, d5                @ q2 = v0v0v1v1v2v2...         \n\t"
        "   vswp q0, q1                  @ now q0 = y q1 = u q2 = v     \n\t"
        "   vst3.8  {d0,d2,d4},[%[dst]]!                                \n\t"
        "   vst3.8  {d1,d3,d5},[%[dst]]!                                \n\t"
        "   sub %[n], %[n], #16                                         \n\t"
        "   cmp %[n], #16                                               \n\t"
        "   bge 0b                                                      \n\t"
        "5: @ end                                                       \n\t"
#ifdef NEEDS_ARM_ERRATA_754319_754320
        "   vmov s0,s0  @ add noop for errata item                      \n\t"
#endif
        : [dst] "+r" (dst), [src] "+r" (src), [n] "+r" (n)
        : [src_stride] "r" (width)
        : "cc", "memory", "q0", "q1", "q2"
        );
    }
}

/* public static functions */
const char* ExifElementsTable::degreesToExifOrientation(const char* degrees) {
    for (unsigned int i = 0; i < ARRAY_SIZE(degress_to_exif_lut); i++) {
        if (!strcmp(degrees, degress_to_exif_lut[i].string1)) {
            return degress_to_exif_lut[i].string2;
        }
    }
    return NULL;
}

void ExifElementsTable::insertExifToJpeg(unsigned char* jpeg, size_t jpeg_size) {
    ReadMode_t read_mode = (ReadMode_t)(READ_METADATA | READ_IMAGE);

    ResetJpgfile();
    if (ReadJpegSectionsFromBuffer(jpeg, jpeg_size, read_mode)) {
        jpeg_opened = true;
        create_EXIF(table, exif_tag_count, gps_tag_count);
    }
}

void ExifElementsTable::saveJpeg(unsigned char* jpeg, size_t jpeg_size) {
    if (jpeg_opened) {
       WriteJpegToBuffer(jpeg, jpeg_size);
       DiscardData();
       jpeg_opened = false;
    }
}

/* public functions */
ExifElementsTable::~ExifElementsTable() {
    int num_elements = gps_tag_count + exif_tag_count;

    for (int i = 0; i < num_elements; i++) {
        if (table[i].Value) {
            free(table[i].Value);
        }
    }

    if (jpeg_opened) {
        DiscardData();
    }
}

status_t ExifElementsTable::insertElement(const char* tag, const char* value) {
    int value_length = 0;
    status_t ret = NO_ERROR;

    if (!value || !tag) {
        return -EINVAL;
    }

    if (position >= MAX_EXIF_TAGS_SUPPORTED) {
        CAMHAL_LOGEA("Max number of EXIF elements already inserted");
        return NO_MEMORY;
    }

    value_length = strlen(value);

    if (IsGpsTag(tag)) {
        table[position].GpsTag = TRUE;
        table[position].Tag = GpsTagNameToValue(tag);
        gps_tag_count++;
    } else {
        table[position].GpsTag = FALSE;
        table[position].Tag = TagNameToValue(tag);
        exif_tag_count++;
    }

    table[position].DataLength = 0;
    table[position].Value = (char*) malloc(sizeof(char) * (value_length + 1));

    if (table[position].Value) {
        strncpy(table[position].Value, value, value_length);
        table[position].DataLength = value_length + 1;
    }

    position++;
    return ret;
}

/* private member functions */
size_t Encoder_libjpeg::encode() {
    jpeg_compress_struct    cinfo;
    jpeg_error_mgr jerr;
    jpeg_destination_mgr jdest;
    uint8_t* row_tmp = NULL;
    uint8_t* row_src = NULL;
    int bpp = 2; // TODO(XXX): hardcoded for uyvy

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    libjpeg_destination_mgr dest_mgr(mDest, mDestSize);

    CAMHAL_LOGDB("encoding...  \n\t"
                 "width: %d    \n\t"
                 "height:%d    \n\t"
                 "dest %p      \n\t"
                 "dest size:%d \n\t"
                 "mSrc %p",
                 mWidth, mHeight, mDest, mDestSize, mSrc);

    cinfo.dest = &dest_mgr;
    cinfo.image_width = mWidth;
    cinfo.image_height = mHeight;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr;
    cinfo.input_gamma = 1;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, mQuality, TRUE);
    cinfo.dct_method = JDCT_IFAST;

    jpeg_start_compress(&cinfo, TRUE);

    row_tmp = (uint8_t*)malloc(mWidth * 3);
    row_src = mSrc;

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row[1];    /* pointer to JSAMPLE row[s] */

        uyvy_to_yuv(row_tmp, (uint32_t*)row_src, mWidth);
        row[0] = row_tmp;
        jpeg_write_scanlines(&cinfo, row, 1);
        row_src = row_src + mWidth*bpp;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return dest_mgr.jpegsize;
}

} // namespace android
