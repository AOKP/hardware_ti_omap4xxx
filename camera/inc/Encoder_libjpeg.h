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
* @file Encoder_libjpeg.h
*
* This defines API for camerahal to encode YUV using libjpeg
*
*/

#ifndef ANDROID_CAMERA_HARDWARE_ENCODER_LIBJPEG_H
#define ANDROID_CAMERA_HARDWARE_ENCODER_LIBJPEG_H

#include <utils/threads.h>
#include <utils/RefBase.h>

namespace android {

/**
 * libjpeg encoder class - uses libjpeg to encode yuv
 */

typedef void (*encoder_libjpeg_callback_t) (size_t jpeg_size,
                                            uint8_t* src,
                                            CameraFrame::FrameType type,
                                            void* cookie1,
                                            void* cookie2,
                                            void* cookie3);

class Encoder_libjpeg : public Thread {
    public:
        Encoder_libjpeg(uint8_t* src,
                               int src_size,
                               uint8_t* dst,
                               int dst_size,
                               int quality,
                               int width,
                               int height,
                               encoder_libjpeg_callback_t cb,
                               CameraFrame::FrameType type,
                               void* cookie1,
                               void* cookie2,
                               void* cookie3)
            : Thread(false), mSrc(src), mDest(dst), mSrcSize(src_size), mDestSize(dst_size),
              mQuality(quality), mWidth(width), mHeight(height), mCb(cb), mCookie1(cookie1),
              mCookie2(cookie2), mCookie3(cookie3), mType(type) {
            this->incStrong(this);
        }

        ~Encoder_libjpeg() {
        }

        virtual bool threadLoop() {
            size_t size = encode();
            mCb(size, mSrc, mType, mCookie1, mCookie2, mCookie3);
            // encoder thread runs, self-destructs, and then exits
            this->decStrong(this);
            return false;
        }

    private:
        uint8_t* mSrc;
        uint8_t* mDest;
        int mSrcSize, mDestSize;
        int mQuality, mWidth, mHeight;
        encoder_libjpeg_callback_t mCb;
        void* mCookie1;
        void* mCookie2;
        void* mCookie3;
        CameraFrame::FrameType mType;

        size_t encode();
};

}

#endif
