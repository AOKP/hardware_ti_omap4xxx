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

namespace android {

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
    // TODO(XXX): optimize later
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
