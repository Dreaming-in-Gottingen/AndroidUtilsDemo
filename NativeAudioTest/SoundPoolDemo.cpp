#define LOG_TAG "SoundPoolDemo"

#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>
#include <fcntl.h>

#include <utils/Log.h>
#include <media/SoundPool.h>

using namespace android;

class SoundPoolDemo : public RefBase {
public:
    SoundPoolDemo(int cnt) {
        ALOGD("SoundPoolDemo ctor");
        mCurLoadedCnt = 0;
        mpSndPol = new SoundPool(cnt, AUDIO_STREAM_MUSIC, 0);
        mMaxStreamCnt = cnt;
    }
    ~SoundPoolDemo() {
        ALOGD("SoundPoolDemo dtor");
        delete mpSndPol;
    }

    void loadFile(char *path) {
        int fd = open(path, O_RDONLY);
        struct stat st;
        fstat(fd, &st);
        int idx = mpSndPol->load(fd, 0, st.st_size, 1);
        close(fd);
        mCurLoadedCnt++;
        ALOGD("stream_id:%d", idx);
    }

    void stop(int id) {
        mpSndPol->stop(id);
    }

    void setCallback(SoundPoolCallback* cb) {
        mpSndPol->setCallback(cb, this);
    }

private:
    SoundPool *mpSndPol;
    int mMaxStreamCnt;
    int mCurLoadedCnt;
};

void SoundPoolDemoCallback(SoundPoolEvent event, SoundPool* soundPool, void* user)
{
    SoundPoolDemo* pSndPolDemo = (SoundPoolDemo*)user;
    int stream_idx;
    switch (event.mMsg) {
    case SoundPoolEvent::SAMPLE_LOADED:
        stream_idx = soundPool->play(event.mArg1, 1.0f, 1.0f, 1, 1, 1.0f);
        ALOGD("Callback:LOADED! mArg1:%d, mArg2:%d, stream_id:%d", event.mArg1, event.mArg2, stream_idx);
        break;
    case SoundPoolEvent::INVALID:
        ALOGE("Callback:INVALID! check why!");
        break;
    }
}

int main(int argc, char**argv)
{
    ALOGD("---------SoundPoolDemo begin----------");
    ALOGD("function: play audio(compressed/uncompressed) at the same time");

    if (argc < 2)
    {
        ALOGE("argc should larger than 1! format: SoundPoolDemo file1...");
        return -1;
    }

    sp<SoundPoolDemo> sp_spd = new SoundPoolDemo(argc-1);
    sp_spd->setCallback(SoundPoolDemoCallback);
    for (int i=1; i<argc; i++)
        sp_spd->loadFile(argv[i]);

    sleep(60);

    for (int i=1; i<argc; i++)
        sp_spd->stop(i);

    sp_spd.clear();

    ALOGD("---------SoundPoolDemo end----------");

    return 0;
}
