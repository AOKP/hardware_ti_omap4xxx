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




#define LOG_TAG "CameraHAL"


#include "CameraHal.h"
#include "VideoMetadata.h"
#include <MetadataBufferType.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>


namespace android {

const int AppCallbackNotifier::NOTIFIER_TIMEOUT = -1;

/*--------------------NotificationHandler Class STARTS here-----------------------------*/

/**
  * NotificationHandler class
  */


///Initialization function for AppCallbackNotifier
status_t AppCallbackNotifier::initialize()
{
    LOG_FUNCTION_NAME;

    mMeasurementEnabled = false;

    ///Create the app notifier thread
    mNotificationThread = new NotificationThread(this);
    if(!mNotificationThread.get())
        {
        CAMHAL_LOGEA("Couldn't create Notification thread");
        return NO_MEMORY;
        }

    ///Start the display thread
    status_t ret = mNotificationThread->run("NotificationThread", PRIORITY_URGENT_DISPLAY);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEA("Couldn't run NotificationThread");
        mNotificationThread.clear();
        return ret;
        }

    mUseMetaDataBufferMode = true;

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

void AppCallbackNotifier::setCallbacks(CameraHal* cameraHal,
                                        camera_notify_callback notify_cb,
                                        camera_data_callback data_cb,
                                        camera_data_timestamp_callback data_cb_timestamp,
                                        camera_request_memory get_memory,
                                        void *user)
{
    Mutex::Autolock lock(mLock);

    LOG_FUNCTION_NAME;

    mCameraHal = cameraHal;
    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mRequestMemory = get_memory;
    mCallbackCookie = user;

    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::setMeasurements(bool enable)
{
    Mutex::Autolock lock(mLock);

    LOG_FUNCTION_NAME;

    mMeasurementEnabled = enable;

    if (  enable  )
        {
         mFrameProvider->enableFrameNotification(CameraFrame::FRAME_DATA_SYNC);
        }

    LOG_FUNCTION_NAME_EXIT;
}


//All sub-components of Camera HAL call this whenever any error happens
void AppCallbackNotifier::errorNotify(int error)
{
    LOG_FUNCTION_NAME;

    CAMHAL_LOGEB("AppCallbackNotifier received error %d", error);

    ///Notify errors to application in callback thread. Post error event to event queue
    TIUTILS::Message msg;
    msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_ERROR;
    msg.arg1 = (void*)error;

    mEventQ.put(&msg);

    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::notificationThread()
{
    bool shouldLive = true;
    status_t ret;

    LOG_FUNCTION_NAME;

    while(shouldLive)
        {
        //CAMHAL_LOGDA("Notification Thread waiting for message");
        ret = TIUTILS::MessageQueue::waitForMsg(&mNotificationThread->msgQ(),
                                                &mEventQ,
                                                &mFrameQ,
                                                AppCallbackNotifier::NOTIFIER_TIMEOUT);

        //CAMHAL_LOGDA("Notification Thread received message");

        if(mNotificationThread->msgQ().hasMsg())
            {
            ///Received a message from CameraHal, process it
            CAMHAL_LOGDA("Notification Thread received message from Camera HAL");
            shouldLive = processMessage();
            if(!shouldLive)
                {
                CAMHAL_LOGDA("Notification Thread exiting.");
                }
            }
        if(mEventQ.hasMsg())
            {
            ///Received an event from one of the event providers
            CAMHAL_LOGDA("Notification Thread received an event from event provider (CameraAdapter)");
            notifyEvent();
            }
        if(mFrameQ.hasMsg())
            {
            ///Received a frame from one of the frame providers
            //CAMHAL_LOGDA("Notification Thread received a frame from frame provider (CameraAdapter)");
            notifyFrame();
            }
        }

    CAMHAL_LOGDA("Notification Thread exited.");
    LOG_FUNCTION_NAME_EXIT;

}

void AppCallbackNotifier::notifyEvent()
{
    ///Receive and send the event notifications to app
    TIUTILS::Message msg;
    LOG_FUNCTION_NAME;
    mEventQ.get(&msg);
    bool ret = true;
    CameraHalEvent *evt = NULL;
    CameraHalEvent::FocusEventData *focusEvtData;
    CameraHalEvent::ZoomEventData *zoomEvtData;
    CameraHalEvent::FaceEventData faceEvtData;

    if(mNotifierState != AppCallbackNotifier::NOTIFIER_STARTED)
    {
        return;
    }

    switch(msg.command)
        {
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_EVENT:

            evt = ( CameraHalEvent * ) msg.arg1;

            if ( NULL == evt )
                {
                CAMHAL_LOGEA("Invalid CameraHalEvent");
                return;
                }

            switch(evt->mEventType)
                {
                case CameraHalEvent::EVENT_SHUTTER:

                    if ( ( NULL != mCameraHal ) &&
                          ( NULL != mNotifyCb ) &&
                          ( mCameraHal->msgTypeEnabled(CAMERA_MSG_SHUTTER) ) )
                        {
                            mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);
                        }

                    break;

                case CameraHalEvent::EVENT_FOCUS_LOCKED:
                case CameraHalEvent::EVENT_FOCUS_ERROR:

                    focusEvtData = &evt->mEventData->focusEvent;
                    if ( ( focusEvtData->focusLocked ) &&
                          ( NULL != mCameraHal ) &&
                          ( NULL != mNotifyCb ) &&
                          ( mCameraHal->msgTypeEnabled(CAMERA_MSG_FOCUS) ) )
                        {
                         mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);
                        }
                    else if ( focusEvtData->focusError &&
                                ( NULL != mCameraHal ) &&
                                ( NULL != mNotifyCb ) &&
                                ( mCameraHal->msgTypeEnabled(CAMERA_MSG_FOCUS) ) )
                        {
                         mNotifyCb(CAMERA_MSG_FOCUS, false, 0, mCallbackCookie);
                        }

                    break;

                case CameraHalEvent::EVENT_ZOOM_INDEX_REACHED:

                    zoomEvtData = &evt->mEventData->zoomEvent;

                    if ( ( NULL != mCameraHal ) &&
                         ( NULL != mNotifyCb) &&
                         ( mCameraHal->msgTypeEnabled(CAMERA_MSG_ZOOM) ) )
                        {
                        mNotifyCb(CAMERA_MSG_ZOOM, zoomEvtData->currentZoomIndex, zoomEvtData->targetZoomIndexReached, mCallbackCookie);
                        }

                    break;

                case CameraHalEvent::EVENT_FACE:

                    faceEvtData = evt->mEventData->faceEvent;

                    if ( ( NULL != mCameraHal ) &&
                         ( NULL != mNotifyCb) &&
                         ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) ) )
                        {
                        // WA for an issue inside CameraService
                        camera_memory_t *tmpBuffer = mRequestMemory(-1, 1, 1, NULL);

                        mDataCb(CAMERA_MSG_PREVIEW_METADATA,
                                tmpBuffer,
                                0,
                                faceEvtData->getFaceResult(),
                                mCallbackCookie);

                        faceEvtData.clear();

                        if ( NULL != tmpBuffer ) {
                            tmpBuffer->release(tmpBuffer);
                        }

                        }

                    break;

                case CameraHalEvent::ALL_EVENTS:
                    break;
                default:
                    break;
                }

            break;

        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_ERROR:

            if (  ( NULL != mCameraHal ) &&
                  ( NULL != mNotifyCb ) &&
                  ( mCameraHal->msgTypeEnabled(CAMERA_MSG_ERROR) ) )
                {
                mNotifyCb(CAMERA_MSG_ERROR, CAMERA_ERROR_UNKNOWN, 0, mCallbackCookie);
                }

            break;

        }

    if ( NULL != evt )
        {
        delete evt;
        }


    LOG_FUNCTION_NAME_EXIT;

}

static void copy2Dto1D(void *dst,
                       void *src,
                       int width,
                       int height,
                       size_t stride,
                       uint32_t offset,
                       unsigned int bytesPerPixel,
                       size_t length,
                       const char *pixelFormat)
{
    unsigned int alignedRow, row;
    unsigned char *bufferDst, *bufferSrc;
    unsigned char *bufferDstEnd, *bufferSrcEnd;
    uint16_t *bufferSrc_UV;
    void *y_uv[2];     //y_uv[0]=> y pointer; y_uv[1]=>uv pointer

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds;

    bounds.left = offset % stride;
    bounds.top = offset / stride;
    bounds.right = width;
    bounds.bottom = height;

    // get the y & uv pointers from the gralloc handle;
    mapper.lock((buffer_handle_t)src,
                GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_NEVER,
                bounds, y_uv);

    CAMHAL_LOGDB("copy2Dto1D() y= %p ; uv=%p.",y_uv[0],y_uv[1]);
    CAMHAL_LOGDB("pixelFormat,= %d; offset=%d",*pixelFormat,offset);

    if (pixelFormat!=NULL) {
        if (strcmp(pixelFormat, CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
            bytesPerPixel = 2;
        } else if (strcmp(pixelFormat, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0 ||
                   strcmp(pixelFormat, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
            bytesPerPixel = 1;
            bufferDst = ( unsigned char * ) dst;
            bufferDstEnd = ( unsigned char * ) dst + width*height*bytesPerPixel;
            bufferSrc = ( unsigned char * ) y_uv[0] + offset;
            bufferSrcEnd = ( unsigned char * ) ( ( size_t ) y_uv[0] + length + offset);
            row = width*bytesPerPixel;
            alignedRow = stride-width;
            int stride_bytes = stride / 8;
            uint32_t xOff = offset % stride;
            uint32_t yOff = offset / stride;

            // going to convert from NV12 here and return
            // Step 1: Y plane: iterate through each row and copy
            for ( int i = 0 ; i < height ; i++) {
                memcpy(bufferDst, bufferSrc, row);
                bufferSrc += stride;
                bufferDst += row;
                if ( ( bufferSrc > bufferSrcEnd ) || ( bufferDst > bufferDstEnd ) ) {
                    break;
                }
            }

            bufferSrc_UV = ( uint16_t * ) ((uint8_t*)y_uv[1] + (stride/2)*yOff + xOff);

            if (strcmp(pixelFormat, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
                 uint16_t *bufferDst_UV;

                // Step 2: UV plane: convert NV12 to NV21 by swapping U & V
                bufferDst_UV = (uint16_t *) (((uint8_t*)dst)+row*height);

                for (int i = 0 ; i < height/2 ; i++, bufferSrc_UV += alignedRow/2) {
                    int n = width;
                    asm volatile (
                    "   pld [%[src], %[src_stride], lsl #2]                         \n\t"
                    "   cmp %[n], #32                                               \n\t"
                    "   blt 1f                                                      \n\t"
                    "0: @ 32 byte swap                                              \n\t"
                    "   sub %[n], %[n], #32                                         \n\t"
                    "   vld2.8  {q0, q1} , [%[src]]!                                \n\t"
                    "   vswp q0, q1                                                 \n\t"
                    "   cmp %[n], #32                                               \n\t"
                    "   vst2.8  {q0,q1},[%[dst]]!                                   \n\t"
                    "   bge 0b                                                      \n\t"
                    "1: @ Is there enough data?                                     \n\t"
                    "   cmp %[n], #16                                               \n\t"
                    "   blt 3f                                                      \n\t"
                    "2: @ 16 byte swap                                              \n\t"
                    "   sub %[n], %[n], #16                                         \n\t"
                    "   vld2.8  {d0, d1} , [%[src]]!                                \n\t"
                    "   vswp d0, d1                                                 \n\t"
                    "   cmp %[n], #16                                               \n\t"
                    "   vst2.8  {d0,d1},[%[dst]]!                                   \n\t"
                    "   bge 2b                                                      \n\t"
                    "3: @ Is there enough data?                                     \n\t"
                    "   cmp %[n], #8                                                \n\t"
                    "   blt 5f                                                      \n\t"
                    "4: @ 8 byte swap                                               \n\t"
                    "   sub %[n], %[n], #8                                          \n\t"
                    "   vld2.8  {d0, d1} , [%[src]]!                                \n\t"
                    "   vswp d0, d1                                                 \n\t"
                    "   cmp %[n], #8                                                \n\t"
                    "   vst2.8  {d0[0],d1[0]},[%[dst]]!                             \n\t"
                    "   bge 4b                                                      \n\t"
                    "5: @ end                                                       \n\t"
#ifdef NEEDS_ARM_ERRATA_754319_754320
                    "   vmov s0,s0  @ add noop for errata item                      \n\t"
#endif
                    : [dst] "+r" (bufferDst_UV), [src] "+r" (bufferSrc_UV), [n] "+r" (n)
                    : [src_stride] "r" (stride_bytes)
                    : "cc", "memory", "q0", "q1"
                    );
                }
            } else if (strcmp(pixelFormat, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
                 uint16_t *bufferDst_U;
                 uint16_t *bufferDst_V;

                // Step 2: UV plane: convert NV12 to YV12 by de-interleaving U & V
                // TODO(XXX): This version of CameraHal assumes NV12 format it set at
                //            camera adapter to support YV12. Need to address for
                //            USBCamera

                bufferDst_V = (uint16_t *) (((uint8_t*)dst)+row*height);
                bufferDst_U = (uint16_t *) (((uint8_t*)dst)+row*height+row*height/4);

                for (int i = 0 ; i < height/2 ; i++, bufferSrc_UV += alignedRow/2) {
                    int n = width;
                    asm volatile (
                    "   pld [%[src], %[src_stride], lsl #2]                         \n\t"
                    "   cmp %[n], #32                                               \n\t"
                    "   blt 1f                                                      \n\t"
                    "0: @ 32 byte swap                                              \n\t"
                    "   sub %[n], %[n], #32                                         \n\t"
                    "   vld2.8  {q0, q1} , [%[src]]!                                \n\t"
                    "   cmp %[n], #32                                               \n\t"
                    "   vst1.8  {q1},[%[dst_v]]!                                    \n\t"
                    "   vst1.8  {q0},[%[dst_u]]!                                    \n\t"
                    "   bge 0b                                                      \n\t"
                    "1: @ Is there enough data?                                     \n\t"
                    "   cmp %[n], #16                                               \n\t"
                    "   blt 3f                                                      \n\t"
                    "2: @ 16 byte swap                                              \n\t"
                    "   sub %[n], %[n], #16                                         \n\t"
                    "   vld2.8  {d0, d1} , [%[src]]!                                \n\t"
                    "   cmp %[n], #16                                               \n\t"
                    "   vst1.8  {d1},[%[dst_v]]!                                    \n\t"
                    "   vst1.8  {d0},[%[dst_u]]!                                    \n\t"
                    "   bge 2b                                                      \n\t"
                    "3: @ Is there enough data?                                     \n\t"
                    "   cmp %[n], #8                                                \n\t"
                    "   blt 5f                                                      \n\t"
                    "4: @ 8 byte swap                                               \n\t"
                    "   sub %[n], %[n], #8                                          \n\t"
                    "   vld2.8  {d0, d1} , [%[src]]!                                \n\t"
                    "   cmp %[n], #8                                                \n\t"
                    "   vst1.8  {d1[0]},[%[dst_v]]!                                 \n\t"
                    "   vst1.8  {d0[0]},[%[dst_u]]!                                 \n\t"
                    "   bge 4b                                                      \n\t"
                    "5: @ end                                                       \n\t"
#ifdef NEEDS_ARM_ERRATA_754319_754320
                    "   vmov s0,s0  @ add noop for errata item                      \n\t"
#endif
                    : [dst_u] "+r" (bufferDst_U), [dst_v] "+r" (bufferDst_V),
                      [src] "+r" (bufferSrc_UV), [n] "+r" (n)
                    : [src_stride] "r" (stride_bytes)
                    : "cc", "memory", "q0", "q1"
                    );
                }
            }
            mapper.unlock((buffer_handle_t)src);
            return ;

        } else if(strcmp(pixelFormat, CameraParameters::PIXEL_FORMAT_RGB565) == 0) {
            bytesPerPixel = 2;
        }
    }

    bufferDst = ( unsigned char * ) dst;
    bufferSrc = ( unsigned char * ) y_uv[0];
    row = width*bytesPerPixel;
    alignedRow = ( row + ( stride -1 ) ) & ( ~ ( stride -1 ) );

    //iterate through each row
    for ( int i = 0 ; i < height ; i++,  bufferSrc += alignedRow, bufferDst += row) {
        memcpy(bufferDst, bufferSrc, row);
    }
    mapper.unlock((buffer_handle_t)src);
}

void AppCallbackNotifier::notifyFrame()
{
    ///Receive and send the frame notifications to app
    TIUTILS::Message msg;
    CameraFrame *frame;
    MemoryHeapBase *heap;
    MemoryBase *buffer = NULL;
    sp<MemoryBase> memBase;
    void *buf = NULL;

    LOG_FUNCTION_NAME;

    if(!mFrameQ.isEmpty())
        {
    mFrameQ.get(&msg);
        }
    else
        {
        return;
        }

    bool ret = true;

    if(mNotifierState != AppCallbackNotifier::NOTIFIER_STARTED)
    {
        return;
    }

    frame = NULL;
    switch(msg.command)
        {
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_FRAME:

                frame = (CameraFrame *) msg.arg1;
                if(!frame)
                    {
                    break;
                    }

                if ( (CameraFrame::RAW_FRAME == frame->mFrameType )&&
                    ( NULL != mCameraHal ) &&
                    ( NULL != mDataCb) &&
                    ( mCameraHal->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE) ) )
                    {

#ifdef COPY_IMAGE_BUFFER

                    camera_memory_t* raw_picture = mRequestMemory(-1, frame->mLength, 1, NULL);

                    if ( NULL != raw_picture )
                    {
                        buf = raw_picture->data;
                        if ( NULL != buf )
                            {
                            memcpy(buf,
                                   ( void * ) ( (unsigned int) frame->mBuffer + frame->mOffset),
                                   frame->mLength);
                            }
                        mFrameProvider->returnFrame(frame->mBuffer,
                                                    ( CameraFrame::FrameType ) frame->mFrameType);
                    }

                    mDataCb(CAMERA_MSG_RAW_IMAGE, raw_picture, 0, NULL, mCallbackCookie);
#else

                     //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase

#endif
                    if(raw_picture)
                        {
                        raw_picture->release(raw_picture);
                        }

                    }
                else if ( ( CameraFrame::IMAGE_FRAME == frame->mFrameType ) &&
                             ( NULL != mCameraHal ) &&
                             ( NULL != mDataCb) )
                    {
                    Mutex::Autolock lock(mLock);

#ifdef COPY_IMAGE_BUFFER

                        camera_memory_t* raw_picture = mRequestMemory(-1, frame->mLength, 1, NULL);

                        if(raw_picture)
                            {
                            buf = raw_picture->data;
                            }

                        if ( NULL != buf)
                            {
                            memcpy(buf,
                                   ( void * ) ( (unsigned int) frame->mBuffer + frame->mOffset),
                                   frame->mLength);
                            }

                        {
                            Mutex::Autolock lock(mBurstLock);
#if 0 //TODO: enable burst mode later
                            if ( mBurst )
                            {
                                `(CAMERA_MSG_BURST_IMAGE, JPEGPictureMemBase, mCallbackCookie);
                            }
                            else
#endif
                            {
                                mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, raw_picture,
                                        0, NULL,
                                        mCallbackCookie);
                            }
                        }
#else

                     //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase

#endif
                        if(raw_picture)
                            {
                            raw_picture->release(raw_picture);
                            }

                        mFrameProvider->returnFrame(frame->mBuffer,
                                                    ( CameraFrame::FrameType ) frame->mFrameType);

                    }
                else if ( ( CameraFrame::VIDEO_FRAME_SYNC == frame->mFrameType ) &&
                             ( NULL != mCameraHal ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)  ) )
                    {
                    mRecordingLock.lock();
                    if(mRecording)
                        {
                        if(mUseMetaDataBufferMode)
                            {
                            camera_memory_t *videoMedatadaBufferMemory =
                                             (camera_memory_t *) mVideoMetadataBufferMemoryMap.valueFor((uint32_t) frame->mBuffer);
                            video_metadata_t *videoMetadataBuffer = (video_metadata_t *) videoMedatadaBufferMemory->data;

                            if( (NULL == videoMedatadaBufferMemory) || (NULL == videoMetadataBuffer) || (NULL == frame->mBuffer) )
                                {
                                CAMHAL_LOGEA("Error! One of the video buffers is NULL");
                                break;
                                }

                            videoMetadataBuffer->metadataBufferType = (int) kMetadataBufferTypeCameraSource;
                            videoMetadataBuffer->handle = frame->mBuffer;
                            videoMetadataBuffer->offset = frame->mOffset;

                            CAMHAL_LOGVB("mDataCbTimestamp : frame->mBuffer=0x%x, videoMetadataBuffer=0x%x, videoMedatadaBufferMemory=0x%x",
                                            frame->mBuffer, videoMetadataBuffer, videoMedatadaBufferMemory);

                            mDataCbTimestamp(frame->mTimestamp, CAMERA_MSG_VIDEO_FRAME,
                                                videoMedatadaBufferMemory, 0, mCallbackCookie);
                            }
                        else
                            {
                            //TODO: Need to revisit this, should ideally be mapping the TILER buffer using mRequestMemory
                            camera_memory_t* fakebuf = mRequestMemory(-1, 4, 1, NULL);
                            if( (NULL == fakebuf) || ( NULL == fakebuf->data) || ( NULL == frame->mBuffer))
                                {
                                CAMHAL_LOGEA("Error! One of the video buffers is NULL");
                                break;
                                }

                            fakebuf->data = frame->mBuffer;
                            mDataCbTimestamp(frame->mTimestamp, CAMERA_MSG_VIDEO_FRAME, fakebuf, 0, mCallbackCookie);
                            fakebuf->release(fakebuf);
                            }
                        }
                    mRecordingLock.unlock();

                    }
                else if(( CameraFrame::SNAPSHOT_FRAME == frame->mFrameType ) &&
                             ( NULL != mCameraHal ) &&
                             ( NULL != mDataCb) &&
                             ( NULL != mNotifyCb)) {
                    Mutex::Autolock lock(mLock);
                    //When enabled, measurement data is sent instead of video data
                    if ( !mMeasurementEnabled ) {
                        if (!mPreviewMemory || !frame->mBuffer) {
                            CAMHAL_LOGDA("Error! One of the buffer is NULL");
                            break;
                        }

                        buf = (void*) mPreviewBufs[mPreviewBufCount];

                        CAMHAL_LOGVB("%d:copy2Dto1D(%p, %p, %d, %d, %d, %d, %d,%s)",
                                     __LINE__,
                                     buf,
                                     frame->mBuffer,
                                     frame->mWidth,
                                     frame->mHeight,
                                     frame->mAlignment,
                                     2,
                                     frame->mLength,
                                     mPreviewPixelFormat);

                        if ( NULL != buf ) {
                            copy2Dto1D(buf,
                                       frame->mBuffer,
                                       frame->mWidth,
                                       frame->mHeight,
                                       frame->mAlignment,
                                       frame->mOffset,
                                       2,
                                       frame->mLength,
                                       mPreviewPixelFormat);
                        }

                        if (mCameraHal->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                            ///Give preview callback to app
                            mDataCb(CAMERA_MSG_POSTVIEW_FRAME, mPreviewMemory, mPreviewBufCount, NULL, mCallbackCookie);
                        }

                        // increment for next buffer
                        mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;
                    }

                    mFrameProvider->returnFrame(frame->mBuffer,
                                                ( CameraFrame::FrameType ) frame->mFrameType);
                } else if ( ( CameraFrame::PREVIEW_FRAME_SYNC== frame->mFrameType ) &&
                            ( NULL != mCameraHal ) &&
                            ( NULL != mDataCb) &&
                            ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) ) {
                    Mutex::Autolock lock(mLock);
                    //When enabled, measurement data is sent instead of video data
                    if ( !mMeasurementEnabled ) {
                        if (!mPreviewMemory || !frame->mBuffer) {
                            CAMHAL_LOGDA("Error! One of the buffer is NULL");
                            break;
                        }

                        buf = (void*) mPreviewBufs[mPreviewBufCount];

                        CAMHAL_LOGVB("%d:copy2Dto1D(%p, %p, %d, %d, %d, %d, %d,%s)",
                                     __LINE__,
                                     buf,
                                     frame->mBuffer,
                                     frame->mWidth,
                                     frame->mHeight,
                                     frame->mAlignment,
                                     2,
                                     frame->mLength,
                                     mPreviewPixelFormat);

                        if ( NULL != buf ) {
                            copy2Dto1D(buf,
                                       frame->mBuffer,
                                       frame->mWidth,
                                       frame->mHeight,
                                       frame->mAlignment,
                                       frame->mOffset,
                                       2,
                                       frame->mLength,
                                       mPreviewPixelFormat);
                        }

                        // Give preview callback to app
                        mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewMemory, mPreviewBufCount, NULL, mCallbackCookie);

                        // increment for next buffer
                        mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;
                    }
                    mFrameProvider->returnFrame(frame->mBuffer,
                                                ( CameraFrame::FrameType ) frame->mFrameType);
                } else if ( ( CameraFrame::FRAME_DATA_SYNC == frame->mFrameType ) &&
                            ( NULL != mCameraHal ) &&
                            ( NULL != mDataCb) &&
                            ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) ) {
                    if (!mPreviewMemory || !frame->mBuffer) {
                        CAMHAL_LOGDA("Error! One of the buffer is NULL");
                        break;
                    }

                    buf = (void*) mPreviewBufs[mPreviewBufCount];
                    if (buf) {
                        if ( (mPreviewMemory->size / MAX_BUFFERS) >= frame->mLength ) {
                            memcpy(buf, ( void * )  frame->mBuffer, frame->mLength);
                        } else {
                            memset(buf, 0, (mPreviewMemory->size / MAX_BUFFERS));
                        }
                    }

                    // Give preview callback to app
                    mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewMemory, mPreviewBufCount, NULL, mCallbackCookie);

                    //Increment the buffer count
                    mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;

                    mFrameProvider->returnFrame(frame->mBuffer,
                                                ( CameraFrame::FrameType ) frame->mFrameType);
                } else {
                    mFrameProvider->returnFrame(frame->mBuffer,
                                                ( CameraFrame::FrameType ) frame->mFrameType);
                    CAMHAL_LOGDB("Frame type 0x%x is still unsupported!", frame->mFrameType);
                }

                break;

        default:

            break;

        };

exit:

    if ( NULL != frame )
        {
        delete frame;
        }

    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::frameCallbackRelay(CameraFrame* caFrame)
{
    LOG_FUNCTION_NAME;
    AppCallbackNotifier *appcbn = (AppCallbackNotifier*) (caFrame->mCookie);
    appcbn->frameCallback(caFrame);
    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::frameCallback(CameraFrame* caFrame)
{
    ///Post the event to the event queue of AppCallbackNotifier
    TIUTILS::Message msg;
    CameraFrame *frame;

    LOG_FUNCTION_NAME;

    if ( NULL != caFrame )
        {

        frame = new CameraFrame(*caFrame);
        if ( NULL != frame )
            {
            msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_FRAME;
            msg.arg1 = frame;
            mFrameQ.put(&msg);
            }
        else
            {
            CAMHAL_LOGEA("Not enough resources to allocate CameraFrame");
            }

        }

    LOG_FUNCTION_NAME_EXIT;
}


void AppCallbackNotifier::eventCallbackRelay(CameraHalEvent* chEvt)
{
    LOG_FUNCTION_NAME;
    AppCallbackNotifier *appcbn = (AppCallbackNotifier*) (chEvt->mCookie);
    appcbn->eventCallback(chEvt);
    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::eventCallback(CameraHalEvent* chEvt)
{

    ///Post the event to the event queue of AppCallbackNotifier
    TIUTILS::Message msg;
    CameraHalEvent *event;


    LOG_FUNCTION_NAME;

    if ( NULL != chEvt )
        {

        event = new CameraHalEvent(*chEvt);
        if ( NULL != event )
            {
            msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_EVENT;
            msg.arg1 = event;
            mEventQ.put(&msg);
            }
        else
            {
            CAMHAL_LOGEA("Not enough resources to allocate CameraHalEvent");
            }

        }

    LOG_FUNCTION_NAME_EXIT;
}


bool AppCallbackNotifier::processMessage()
{
    ///Retrieve the command from the command queue and process it
    TIUTILS::Message msg;

    LOG_FUNCTION_NAME;

    CAMHAL_LOGDA("+Msg get...");
    mNotificationThread->msgQ().get(&msg);
    CAMHAL_LOGDA("-Msg get...");
    bool ret = true;

    switch(msg.command)
      {
        case NotificationThread::NOTIFIER_EXIT:
          {
            CAMHAL_LOGEA("Received NOTIFIER_EXIT command from Camera HAL");
            mNotifierState = AppCallbackNotifier::NOTIFIER_EXITED;
            ret = false;
            break;
          }
        default:
          {
            CAMHAL_LOGEA("Error: ProcessMsg() command from Camera HAL");
            break;
          }
      }

    LOG_FUNCTION_NAME_EXIT;

    return ret;


}

AppCallbackNotifier::~AppCallbackNotifier()
{
    LOG_FUNCTION_NAME;

    ///Stop app callback notifier if not already stopped
    stop();

    ///Unregister with the frame provider
    if ( NULL != mFrameProvider )
        {
        mFrameProvider->disableFrameNotification(CameraFrame::ALL_FRAMES);
        }

    //unregister with the event provider
    if ( NULL != mEventProvider )
        {
        mEventProvider->disableEventNotification(CameraHalEvent::ALL_EVENTS);
        }

    TIUTILS::Message msg = {0,0,0,0,0,0};
    msg.command = NotificationThread::NOTIFIER_EXIT;

    ///Post the message to display thread
    mNotificationThread->msgQ().put(&msg);

    //Exit and cleanup the thread
    mNotificationThread->requestExitAndWait();

    //Delete the display thread
    mNotificationThread.clear();


    ///Free the event and frame providers
    if ( NULL != mEventProvider )
        {
        ///Deleting the event provider
        CAMHAL_LOGDA("Stopping Event Provider");
        delete mEventProvider;
        mEventProvider = NULL;
        }

    if ( NULL != mFrameProvider )
        {
        ///Deleting the frame provider
        CAMHAL_LOGDA("Stopping Frame Provider");
        delete mFrameProvider;
        mFrameProvider = NULL;
        }

    releaseSharedVideoBuffers();

    LOG_FUNCTION_NAME_EXIT;
}

//Free all video heaps and buffers
void AppCallbackNotifier::releaseSharedVideoBuffers()
{
    LOG_FUNCTION_NAME;

    if(mUseMetaDataBufferMode)
    {
        camera_memory_t* videoMedatadaBufferMemory;
        for (unsigned int i = 0; i < mVideoMetadataBufferMemoryMap.size();  i++)
            {
            videoMedatadaBufferMemory = (camera_memory_t*) mVideoMetadataBufferMemoryMap.valueAt(i);
            if(NULL != videoMedatadaBufferMemory)
                {
                videoMedatadaBufferMemory->release(videoMedatadaBufferMemory);
                CAMHAL_LOGDB("Released  videoMedatadaBufferMemory=0x%x", videoMedatadaBufferMemory);
                }
            }

        mVideoMetadataBufferMemoryMap.clear();
        mVideoMetadataBufferReverseMap.clear();
    }

    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::setEventProvider(int32_t eventMask, MessageNotifier * eventNotifier)
{

    LOG_FUNCTION_NAME;
    ///@remarks There is no NULL check here. We will check
    ///for NULL when we get start command from CameraHal
    ///@Remarks Currently only one event provider (CameraAdapter) is supported
    ///@todo Have an array of event providers for each event bitmask
    mEventProvider = new EventProvider(eventNotifier, this, eventCallbackRelay);
    if ( NULL == mEventProvider )
        {
        CAMHAL_LOGEA("Error in creating EventProvider");
        }
    else
        {
        mEventProvider->enableEventNotification(eventMask);
        }

    LOG_FUNCTION_NAME_EXIT;
}

void AppCallbackNotifier::setFrameProvider(FrameNotifier *frameNotifier)
{
    LOG_FUNCTION_NAME;
    ///@remarks There is no NULL check here. We will check
    ///for NULL when we get the start command from CameraAdapter
    mFrameProvider = new FrameProvider(frameNotifier, this, frameCallbackRelay);
    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Error in creating FrameProvider");
        }
    else
        {
        //Register only for captured images and RAW for now
        //TODO: Register for and handle all types of frames
        mFrameProvider->enableFrameNotification(CameraFrame::IMAGE_FRAME);
        mFrameProvider->enableFrameNotification(CameraFrame::RAW_FRAME);
        }

    LOG_FUNCTION_NAME_EXIT;
}

status_t AppCallbackNotifier::startPreviewCallbacks(CameraParameters &params, void *buffers, uint32_t *offsets, int fd, size_t length, size_t count)
{
    sp<MemoryHeapBase> heap;
    sp<MemoryBase> buffer;
    unsigned int *bufArr;
    size_t size = 0;

    LOG_FUNCTION_NAME;

    Mutex::Autolock lock(mLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to start video recording without FrameProvider");
        return -EINVAL;
        }

    if ( mPreviewing )
        {
        CAMHAL_LOGDA("+Already previewing");
        return NO_INIT;
        }

    int w,h;
    ///Get preview size
    params.getPreviewSize(&w, &h);

    //Get the preview pixel format
    mPreviewPixelFormat = params.getPreviewFormat();

     if(strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
        {
        size = w*h*2;
        mPreviewPixelFormat = CameraParameters::PIXEL_FORMAT_YUV422I;
        }
    else if(strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0 ||
            strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV420P) == 0)
        {
        size = (w*h*3)/2;
        mPreviewPixelFormat = CameraParameters::PIXEL_FORMAT_YUV420SP;
        }
    else if(strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
        {
        size = w*h*2;
        mPreviewPixelFormat = CameraParameters::PIXEL_FORMAT_RGB565;
        }

    mPreviewMemory = mRequestMemory(-1, size, AppCallbackNotifier::MAX_BUFFERS, NULL);
    if (!mPreviewMemory) {
        return NO_MEMORY;
    }

    for (int i=0; i < AppCallbackNotifier::MAX_BUFFERS; i++) {
        mPreviewBufs[i] = (unsigned char*) mPreviewMemory->data + (i*size);
    }

    if ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME ) ) {
         mFrameProvider->enableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
    }

    mPreviewBufCount = 0;

    mPreviewing = true;

    LOG_FUNCTION_NAME;

    return NO_ERROR;
}

void AppCallbackNotifier::setBurst(bool burst)
{
    LOG_FUNCTION_NAME;

    Mutex::Autolock lock(mBurstLock);

    mBurst = burst;

    LOG_FUNCTION_NAME_EXIT;
}

status_t AppCallbackNotifier::stopPreviewCallbacks()
{
    sp<MemoryHeapBase> heap;
    sp<MemoryBase> buffer;

    LOG_FUNCTION_NAME;

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop preview callbacks without FrameProvider");
        return -EINVAL;
        }

    if ( !mPreviewing )
        {
        return NO_INIT;
        }

    mFrameProvider->disableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);

    mPreviewMemory->release(mPreviewMemory);

    mPreviewing = false;

    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;

}

status_t AppCallbackNotifier::useMetaDataBufferMode(bool enable)
{
    mUseMetaDataBufferMode = enable;

    return NO_ERROR;
}


status_t AppCallbackNotifier::startRecording()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME;

   Mutex::Autolock lock(mRecordingLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to start video recording without FrameProvider");
        ret = -1;
        }

    if(mRecording)
        {
        return NO_INIT;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->enableFrameNotification(CameraFrame::VIDEO_FRAME_SYNC);
        }

    mRecording = true;

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

//Allocate metadata buffers for video recording
status_t AppCallbackNotifier::initSharedVideoBuffers(void *buffers, uint32_t *offsets, int fd, size_t length, size_t count)
{
    status_t ret = NO_ERROR;
    LOG_FUNCTION_NAME;

    if(mUseMetaDataBufferMode)
        {
        uint32_t *bufArr = NULL;
        camera_memory_t* videoMedatadaBufferMemory = NULL;

        if(NULL == buffers)
            {
            CAMHAL_LOGEA("Error! Video buffers are NULL");
            return BAD_VALUE;
            }
        bufArr = (uint32_t *) buffers;

        for (uint32_t i = 0; i < count; i++)
            {
            videoMedatadaBufferMemory = mRequestMemory(-1, sizeof(video_metadata_t), 1, NULL);
            if((NULL == videoMedatadaBufferMemory) || (NULL == videoMedatadaBufferMemory->data))
                {
                CAMHAL_LOGEA("Error! Could not allocate memory for Video Metadata Buffers");
                return NO_MEMORY;
                }

            mVideoMetadataBufferMemoryMap.add(bufArr[i], (uint32_t)(videoMedatadaBufferMemory));
            mVideoMetadataBufferReverseMap.add((uint32_t)(videoMedatadaBufferMemory->data), bufArr[i]);
            CAMHAL_LOGDB("bufArr[%d]=0x%x, videoMedatadaBufferMemory=0x%x, videoMedatadaBufferMemory->data=0x%x",
                    i, bufArr[i], videoMedatadaBufferMemory, videoMedatadaBufferMemory->data);
            }
        }

exit:
    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

status_t AppCallbackNotifier::stopRecording()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME;

    Mutex::Autolock lock(mRecordingLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop video recording without FrameProvider");
        ret = -1;
        }

    if(!mRecording)
        {
        return NO_INIT;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->disableFrameNotification(CameraFrame::VIDEO_FRAME_SYNC);
        }

    ///Release the shared video buffers
    releaseSharedVideoBuffers();

    mRecording = false;

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

status_t AppCallbackNotifier::releaseRecordingFrame(const void* mem)
{
    status_t ret = NO_ERROR;
    void *frame = NULL;

    LOG_FUNCTION_NAME;
    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop video recording without FrameProvider");
        ret = -1;
        }

    if ( NULL == mem )
        {
        CAMHAL_LOGEA("Video Frame released is invalid");
        ret = -1;
        }

    if( NO_ERROR != ret )
        {
        return ret;
        }

    if(mUseMetaDataBufferMode)
        {
        video_metadata_t *videoMetadataBuffer = (video_metadata_t *) mem ;
        frame = (void*) mVideoMetadataBufferReverseMap.valueFor((uint32_t) videoMetadataBuffer);
        CAMHAL_LOGVB("Releasing frame with videoMetadataBuffer=0x%x, videoMetadataBuffer->handle=0x%x & frame handle=0x%x\n",
                       videoMetadataBuffer, videoMetadataBuffer->handle, frame);
        }
    else
        {
        frame = (void*)(*((uint32_t *)mem));
        }

    if ( NO_ERROR == ret )
        {
         ret = mFrameProvider->returnFrame(frame, CameraFrame::VIDEO_FRAME_SYNC);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

status_t AppCallbackNotifier::enableMsgType(int32_t msgType)
{
    if(msgType & CAMERA_MSG_POSTVIEW_FRAME)
        {
        mFrameProvider->enableFrameNotification(CameraFrame::SNAPSHOT_FRAME);
        }

    if(msgType & CAMERA_MSG_PREVIEW_FRAME)
        {
         mFrameProvider->enableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    return NO_ERROR;
}

status_t AppCallbackNotifier::disableMsgType(int32_t msgType)
{
    if(msgType & CAMERA_MSG_POSTVIEW_FRAME)
        {
        mFrameProvider->disableFrameNotification(CameraFrame::SNAPSHOT_FRAME);
        }

    if(msgType & CAMERA_MSG_PREVIEW_FRAME)
        {
        mFrameProvider->disableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    return NO_ERROR;

}

status_t AppCallbackNotifier::start()
{
    LOG_FUNCTION_NAME;
    if(mNotifierState==AppCallbackNotifier::NOTIFIER_STARTED)
        {
        CAMHAL_LOGDA("AppCallbackNotifier already running");
        LOG_FUNCTION_NAME_EXIT;
        return ALREADY_EXISTS;
        }

    ///Check whether initial conditions are met for us to start
    ///A frame provider should be available, if not return error
    if(!mFrameProvider)
        {
        ///AppCallbackNotifier not properly initialized
        CAMHAL_LOGEA("AppCallbackNotifier not properly initialized - Frame provider is NULL");
        LOG_FUNCTION_NAME_EXIT;
        return NO_INIT;
        }

    ///At least one event notifier should be available, if not return error
    ///@todo Modify here when there is an array of event providers
    if(!mEventProvider)
        {
        CAMHAL_LOGEA("AppCallbackNotifier not properly initialized - Event provider is NULL");
        LOG_FUNCTION_NAME_EXIT;
        ///AppCallbackNotifier not properly initialized
        return NO_INIT;
        }

    mNotifierState = AppCallbackNotifier::NOTIFIER_STARTED;
    CAMHAL_LOGDA(" --> AppCallbackNotifier NOTIFIER_STARTED \n");

    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;

}

status_t AppCallbackNotifier::stop()
{
    LOG_FUNCTION_NAME;

    if(mNotifierState!=AppCallbackNotifier::NOTIFIER_STARTED)
        {
        CAMHAL_LOGDA("AppCallbackNotifier already in stopped state");
        LOG_FUNCTION_NAME_EXIT;
        return ALREADY_EXISTS;
        }

    mNotifierState = AppCallbackNotifier::NOTIFIER_STOPPED;
    CAMHAL_LOGDA(" --> AppCallbackNotifier NOTIFIER_STOPPED \n");

    LOG_FUNCTION_NAME_EXIT;
    return NO_ERROR;
}


/*--------------------NotificationHandler Class ENDS here-----------------------------*/



};
