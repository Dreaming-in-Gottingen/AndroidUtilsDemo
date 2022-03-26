/**
 * Test based on AndroidQ.
 * This demo test FrameDecoder usage.
 * It reads local video file, decodes one frame and saves to local file.
 *
 * Usage like: framedecoder_demo dji_720p.mp4 out_pts_30s 30
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "framedecoder_demo"
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>

#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ADebug.h>

#include <include/FrameDecoder.h>

#include <media/stagefright/FileSource.h>
#include <media/IMediaExtractor.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MediaExtractorFactory.h>
#include <media/stagefright/MediaCodecList.h>


using namespace android;
int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("error input! decode frame from input video file at specify pts.\n");
        printf("  usage: %s input_file output_file pts_sec\n", argv[0]);
        return -1;
    }
    const char* videoFilePath = argv[1];
    int ret = access(videoFilePath, F_OK);
    if (ret != 0) {
        printf("video file(%s) not found!\n", videoFilePath);
        return -1;
    }
    FILE *videoFileFp = fopen(videoFilePath, "rb");
    printf("input video file info: path=%s, fp=%p\n", videoFilePath, videoFileFp);

    sp<FileSource> source = new FileSource(videoFilePath);
    status_t err;
    if ((err = source->initCheck()) != OK) {
        printf("source initCheck failed!\n");
        return err;
    }

    sp<IMediaExtractor> extractor = MediaExtractorFactory::Create(source);
    if (extractor == NULL) {
        source.clear();
        printf("extractor create failed!\n");
        return UNKNOWN_ERROR;
    }

    sp<MetaData> fileMeta = extractor->getMetaData();
    if (fileMeta == NULL) {
        printf("extractor doesn't publish metadata, failed to initialize?!\n");
        return -1;
    }
    fileMeta->dumpToLog();

    size_t n = extractor->countTracks();
    printf("countTracks=%zu\n", n);

    ssize_t videoIdx = -1;
    const char *mime;
    for (size_t i=0; i<n; i++) {
        ALOGD("------------track_id=%zu dump info-------------", i);
        sp<MetaData> meta = extractor->getTrackMetaData(i);
        if (!meta) {
            break;
        }

        CHECK(meta->findCString(kKeyMIMEType, &mime));
        if (!strncasecmp(mime, "video/", 6)) {
            printf("got the video track! idx=%zu\n", i);
            videoIdx = i;
        }

        meta->dumpToLog();
        String8 trackInfo = meta->toString();
        ALOGD("track[%zu]: %s", i, trackInfo.c_str());
    }

    if (videoIdx == -1) {
        printf("no video track found for file(%s)!\n", videoFilePath);
        return -1;
    }

    ALOGD("dump video extensive MetaData");
    sp<MetaData> trackExtMeta = extractor->getTrackMetaData(videoIdx, MediaExtractor::kIncludeExtensiveMetaData);
    trackExtMeta->dumpToLog();

    int32_t width, height;
    trackExtMeta->findInt32(kKeyWidth, &width);
    trackExtMeta->findInt32(kKeyHeight, &height);
    ALOGD("video dimension: %d x %d", width, height);

    char rgbaFilePath[128] = "";
    snprintf(rgbaFilePath, 128, "%s_%dx%d.rgba", argv[2], width, height);
    FILE *rgbaFileFp = fopen(rgbaFilePath, "wb");
    if (rgbaFileFp == NULL) {
        printf("rgba file(%s) can not be created!\n", rgbaFilePath);
        return -1;
    } else {
        printf("output file will be: %s, it is a rgba file.\n", rgbaFilePath);
    }

    //sp<IMemory> outFrame = FrameDecoder::getMetadataOnly(trackExtMeta, 0);
    //printf("outFrame=%p\n", outFrame.get());
    //printf("pointer=%p, size=%zu, offset=%zu\n", outFrame->pointer(), outFrame->size(), outFrame->offset());

    CHECK(trackExtMeta->findCString(kKeyMIMEType, &mime));
    sp<IMediaSource> videoSrc = extractor->getTrack(videoIdx);

    //sp<MetaData> vidFmt = videoSrc->getFormat();
    //vidFmt->dumpToLog();

    //bool preferhw = property_get_bool("media.stagefright.thumbnail.prefer_hw_codecs", false);
    //uint32_t flags = preferhw ? 0 : MediaCodecList::kPreferSoftwareCodecs;
    uint32_t flags = MediaCodecList::kPreferSoftwareCodecs;
    Vector<AString> matchingCodecs;
    MediaCodecList::findMatchingCodecs(
            mime,
            false, /* encoder */
            flags,
            &matchingCodecs);
    int64_t timeUs = atoi(argv[3])*1000*1000;
    int numFrames = 1;
    int option = 0;
    int colorFormat = HAL_PIXEL_FORMAT_RGBA_8888;
    for (size_t i=0; i<matchingCodecs.size(); i++) {
        const AString &componentName = matchingCodecs[i];
        printf("componentName[%zu]=%s\n", i, componentName.c_str());
        sp<VideoFrameDecoder> decoder = new VideoFrameDecoder(componentName, trackExtMeta, videoSrc);
        if (decoder->init(timeUs, numFrames, option, colorFormat) == OK) {
            sp<IMemory> outFrame = decoder->extractFrame();
            if (outFrame == NULL) {
                printf("extractFrame failed!\n");
            } else {
                printf("pointer=%p, size=%zu, offset=%zu\n", outFrame->pointer(), outFrame->size(), outFrame->offset());
                fwrite(outFrame->pointer(), 1, outFrame->size() + outFrame->offset(), rgbaFileFp);
                break;
            }
        } else {
            printf("init failed!\n");
        }
    }

    if (source != NULL) {
        source->close();
    }
    fclose(videoFileFp);
    fclose(rgbaFileFp);

    return 0;
}
