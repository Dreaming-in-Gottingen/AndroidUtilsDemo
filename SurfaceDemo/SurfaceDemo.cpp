/*
 * Copyright (C) 2012 The Android Open Source Project
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
 * Test based on AndroidQ.
 * This demo test ANativeWindow usage.
 * It reads local yuv file and renders to device screen.
 *
 * Usage like: surface_demo 1280 720 /mnt/sdcard/dji_720p_nv12.yuv
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "surface_test"
#include <utils/Log.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/SurfaceUtils.h>

#include <gui/SurfaceComposerClient.h>
#include <gui/Surface.h>
#include <ui/DisplayInfo.h>

void fillNV12Buffer(uint8_t* buf, int w, int h, int stride, int idx)
{
    const int blockWidth = w > 16 ? w / 16 : 1;
    const int blockHeight = h > 16 ? h / 16 : 1;
    const int yuvTexOffsetY = 0;
    int yuvTexStrideY = stride;
    int yuvTexOffsetV = yuvTexStrideY * h;
    int yuvTexStrideV = (yuvTexStrideY/2 + 0xf) & ~0xf;
    int yuvTexOffsetU = yuvTexOffsetV + yuvTexStrideV * h / 2;
    int yuvTexStrideU = yuvTexStrideV;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            int parityX = (x / blockWidth) & 1;
            int parityY = (y / blockHeight) & 1;
            unsigned char intensity = (parityX ^ parityY) ? 63 : 191;
            intensity |= (idx<<4);
            buf[yuvTexOffsetY + (y * yuvTexStrideY) + x] = intensity;
            if (x < w / 2 && y < h / 2) {
                buf[yuvTexOffsetU + (y * yuvTexStrideU) + x] = intensity;
                if (x * 2 < w / 2 && y * 2 < h / 2) {
                    buf[yuvTexOffsetV + (y*2 * yuvTexStrideV) + x*2 + 0] =
                    buf[yuvTexOffsetV + (y*2 * yuvTexStrideV) + x*2 + 1] =
                    buf[yuvTexOffsetV + ((y*2+1) * yuvTexStrideV) + x*2 + 0] =
                    buf[yuvTexOffsetV + ((y*2+1) * yuvTexStrideV) + x*2 + 1] =
                         intensity;
                }
            }
        }
    }
}

using namespace android;
int main(int argc, char **argv)
{
    if (argc != 4)
        printf("usage: surface_test width height input.yuv format(default:nv12)\n");
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    const char* yuvPath = argv[3];
    FILE *yuvFp = fopen(yuvPath, "rb");
    printf("input info: size=%dx%d, yuvPath=%s, yuvFp=%p\n", width, height, yuvPath, yuvFp);
    if (yuvFp == NULL) {
        printf("yuv file(%s) not found!\n", yuvPath);
        return -1;
    }

    // get surface
    sp<SurfaceComposerClient> composerClient;
    sp<SurfaceControl> control;
    sp<android::Surface> surface;

    composerClient = new SurfaceComposerClient;
    CHECK_EQ(composerClient->initCheck(), (status_t)OK);

    const sp<IBinder> display = SurfaceComposerClient::getInternalDisplayToken();
    CHECK(display != nullptr);

    DisplayInfo info;
    CHECK_EQ(SurfaceComposerClient::getDisplayInfo(display, &info), NO_ERROR);

    ssize_t displayWidth = info.w;
    ssize_t displayHeight = info.h;
    printf("display resolution is %zd x %zd\n", displayWidth, displayHeight);

    control = composerClient->createSurface(
            String8("surface_test"),
            displayWidth,
            displayHeight,
            PIXEL_FORMAT_RGB_565,
            0);

    CHECK(control != NULL);
    CHECK(control->isValid());

    SurfaceComposerClient::Transaction{}
             .setLayer(control, INT_MAX)
             .show(control)
             .apply();

    surface= control->getSurface();
    CHECK(surface!= NULL);

    // surface init
    uint64_t newId;
    if (surface->getUniqueId(&newId) == NO_ERROR) {
        printf("getUniqueId with newId=%lx\n", newId);
    } else {
        printf("getUniqueId failed!!!\n");
        return -1;
    }

    ANativeWindow *nativeWindow = surface.get();
    status_t err = nativeWindowConnect(nativeWindow, "connectToSurface");
    if (err == OK) {
        nativeWindowDisconnect(nativeWindow, "connectToSurface(reconnect)");
        err = nativeWindowConnect(nativeWindow, "connectToSurface(reconnect)");
    }
    if (err != OK) {
        printf("nativeWindowConnect returned an error(%d): %s\n", err, strerror(-err));
        return -1;
    }

    // 0x13-i420-HAL_PIXEL_FORMAT_YCbCr_420_P:  yyyy...u...v...
    // 0x15-nv12-HAL_PIXEL_FORMAT_YCbCr_420_SP  yyyy...uvuv...
    // 0x1C-yv12-HAL_PIXEL_FORMAT_YCrCb_420_P   yyyy...v...u...
    //printf("YCbCr=%#x, OMX=%#x\n", HAL_PIXEL_FORMAT_YCbCr_420_SP, OMX_COLOR_FormatYUV420SemiPlanar);
    //int usageBits = 0;
    // only 0x15 is supported for unisoc SharkLE
    err = setNativeWindowSizeFormatAndUsage(nativeWindow, width, height, /*format*/0x15, /*rotation*/0, /*usage*/0x2933, true);
    if (err != OK) {
        printf("setNativeWindowSizeFormatAndUsage returned an error(%d): %s\n", err, strerror(-err));
        return -1;
    }

    int minUndequeuedBuffers;
    err = surface->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBuffers);
    if (err != OK) {
        printf("query returned an error: %s (%d)\n", strerror(-err), err);
        return -1;
    }
    printf("minUndequeuedBuffers=%d\n", minUndequeuedBuffers);

    int bufCnt = 12;
    err = native_window_set_buffer_count(nativeWindow, bufCnt);
    if (err != OK) {
        printf("native_window_set_buffer_count returned an error(%d): %s\n", err, strerror(-err));
        return -1;
    }

    // allocate GraphicBuffer, dequeue and queue
    surface->getIGraphicBufferProducer()->allowAllocation(true);
    surface->setDequeueTimeout(-1);
    ANativeWindowBuffer *anw_buf[bufCnt];
    int fenceFd[bufCnt];
    for (int i=0; i<bufCnt; i++) {
        err = nativeWindow->dequeueBuffer(nativeWindow, &anw_buf[i], &fenceFd[i]);
        if (err != 0) {
            printf("dequeueBuffer@[%d] failed with error(%d): %s\n", i, err, strerror(-err));
            break;
        }
        printf("[idx=%d] dequeueBuffer with fenceFd=%d, buf_info: width=%d, height=%d, stride=%d, format=%#x, usage=%#lx\n",
            i, fenceFd[i], anw_buf[i]->width, anw_buf[i]->height, anw_buf[i]->stride, anw_buf[i]->format, anw_buf[i]->usage);

        nativeWindow->queueBuffer(nativeWindow, anw_buf[i], fenceFd[i]);
    }
    surface->getIGraphicBufferProducer()->allowAllocation(false);

    // fill yuv and render
    uint32_t yuvSz = width * height * 3 / 2;
    uint32_t bufIdx = 0;
    uint32_t picIdx = 0;
    while (true) {
        if (bufIdx>=bufCnt)
            bufIdx=0;
        err = nativeWindow->dequeueBuffer(nativeWindow, &anw_buf[bufIdx], &fenceFd[bufIdx]);
        if (err != 0) {
            printf("dequeueBuffer@[%d] failed with error(%d): %s\n", bufIdx, err, strerror(-err));
            break;
        }

        sp<GraphicBuffer> buf(GraphicBuffer::from(anw_buf[bufIdx]));
        uint8_t* img = nullptr;
        buf->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&img));
#if 0
        printf("GB[%d] info: vaddr=%p, wxh=%dx%d, stride=%d, usage=%#lx, pix_fmt=%#x, layer_cnt=%d, id=%#lx\n", bufIdx,
                img,
                buf->getWidth(),
                buf->getHeight(),
                buf->getStride(),
                buf->getUsage(),
                buf->getPixelFormat(),
                buf->getLayerCount(),
                buf->getId()
                );
#endif

        int rdSz = fread(img, 1, yuvSz, yuvFp);
        if (rdSz != yuvSz) {
            printf("EOF found! exit loop!\n");
            break;
        }
        //fillNV12Buffer(img, anw_buf[bufIdx]->width, anw_buf[bufIdx]->height, buf->getStride(), bufIdx);

        buf->unlock();

        nativeWindow->queueBuffer(nativeWindow, anw_buf[bufIdx], fenceFd[bufIdx]);
        printf("renderring picture_%d...\n", picIdx);
        usleep(30*1000);
        bufIdx++;
        picIdx++;
    }

    printf("we have renderred all frames now! free all the resources before exit!\n");
    composerClient->dispose();
    fclose(yuvFp);

    return 0;
}
