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
 * This demo test SoftwareRenderer usage.
 * It reads local yuv file and renders to device screen.
 * Before render, pic will be scaled by libyuv.
 *
 * Usage like: softrenderer_demo 1280 720 /mnt/sdcard/dji_720p_nv12.yuv nv12
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "softwarerenderer_demo"
#include <utils/Log.h>

#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/SurfaceUtils.h>
#include <include/SoftwareRenderer.h>

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
    if (argc != 5) {
        printf("usage: %s width height input.yuv format(nv12 or i420)\n", argv[0]);
        return -1;
    }
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    const char* yuvPath = argv[3];
    FILE *yuvFp = fopen(yuvPath, "rb");
    printf("input info: size=%dx%d, yuvPath=%s, yuvFp=%p, yuvColorFmt=%s\n", width, height, yuvPath, yuvFp, argv[4]);
    if (yuvFp == NULL) {
        printf("yuv file(%s) not found!\n", yuvPath);
        return -1;
    }
    uint32_t colorFmt = OMX_COLOR_FormatYUV420SemiPlanar;
    if (strncmp(argv[4], "nv12", 4) == 0) {
        colorFmt = OMX_COLOR_FormatYUV420SemiPlanar;
        printf("yuv format use OMX_COLOR_FormatYUV420SemiPlanar!\n");
    } else if (strncmp(argv[4], "i420", 4) == 0) {
        colorFmt = OMX_COLOR_FormatYUV420Planar;
        printf("yuv format use OMX_COLOR_FormatYUV420Planar!\n");
    } else {
        colorFmt = OMX_COLOR_FormatYUV420SemiPlanar;
        printf("unknown input yuv format: %s, use default(OMX_COLOR_FormatYUV420SemiPlanar)!\n", argv[4]);
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
            String8("softrenderer_demo"),
            displayWidth/2,
            displayHeight/2,
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

    int32_t rotation = 0;
    SoftwareRenderer *softRender = new SoftwareRenderer(nativeWindow, rotation);

    uint32_t picIdx = 0;
    uint32_t yuvSz = width * height * 3 / 2;
    uint8_t *img = (uint8_t*)malloc(yuvSz);
    while (true) {
        int rdSz = fread(img, 1, yuvSz, yuvFp);
        if (rdSz != yuvSz) {
            printf("rdSz=%d, must be EOF found! exit loop!\n", rdSz);
            break;
        }
        sp<AMessage> format = new AMessage();
        format->setInt32("color-format", colorFmt); // nv12 or i420
        format->setInt32("width", width);
        format->setInt32("slice-height", height);
        format->setInt32("stride", width);
        softRender->render(img, yuvSz, 0, 0, 1, format);
        usleep(33*1000); // 30 fps render speed
        printf("rendering pic_idx=%d\n", picIdx++);
    }

    printf("we have renderred all frames now! free all the resources before exit!\n");
    delete softRender;
    composerClient->dispose();
    fclose(yuvFp);

    return 0;
}
