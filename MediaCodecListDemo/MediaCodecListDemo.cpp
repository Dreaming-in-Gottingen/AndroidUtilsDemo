// debug on Android4.4 by zjz

#define LOG_TAG "MediaCodecListDemo"
#include <utils/Log.h>

#include <utils/Vector.h>

#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodecList.h>

using namespace android;

int main()
{
    ALOGI("-----------MediaCodecListDemo begin---------------");

    const MediaCodecList *pList = MediaCodecList::getInstance();
    if (pList == NULL) {
        ALOGE("MediaCodecList::getInstance fail!");
        return -1;
    }

    ssize_t codecsCnt = pList->countCodecs();
    ALOGD("countCodecs(): %zd", codecsCnt);

    ALOGD("------dump codec name, type, encoder...-------");
    for (ssize_t idx=0; idx<codecsCnt; idx++) {
        Vector<AString> types;
        pList->getSupportedTypes(idx, &types);
        ALOGD("idx: %2zd, name: %s, type: %s, encoder: %d", idx, pList->getCodecName(idx), types[0].c_str(), pList->isEncoder(idx));
    }

    ALOGD("------dump codec type and profile/level...-------");
    for (ssize_t idx=0; idx<codecsCnt; idx++) {
        Vector<AString> types;
        pList->getSupportedTypes(idx, &types);

        Vector<MediaCodecList::ProfileLevel> profileLevel;
        Vector<uint32_t> colorFmt;
        uint32_t flags;
        pList->getCodecCapabilities(idx, types[0].c_str(), &profileLevel, &colorFmt, &flags);

        ALOGD("idx: %2zd, type: %s, profileLevel.size():%d, flags:%#x", idx, types[0].c_str(), profileLevel.size(), flags);
        //ALOGD("idx: %2zd, type: %s, profile: %u, level: %u, colorFmt: %u, flags: %#x", idx,
        //    types[0].c_str(), profileLevel[0].mProfile, profileLevel[0].mLevel, colorFmt[0], flags);
    }

    const char *mime = "video/avc";
    ssize_t idx = pList->findCodecByType(mime, true, 0);
    ALOGD("find idx(%d) for mime(%s)+encoder(true)", idx, mime);

    mime = "audio/mp4a-latm";
    ssize_t retIdx = pList->findCodecByType(mime, true, 0);
    ALOGD("1st find retIdx(%d) for mime(%s)+encoder(true)", retIdx, mime);
    retIdx = pList->findCodecByType(mime, true, retIdx+1);
    ALOGD("2nd find retIdx(%d) for mime(%s)+encoder(true)", retIdx, mime);

    ALOGI("-----------MediaCodecListDemo end---------------");
    return 0;
}
