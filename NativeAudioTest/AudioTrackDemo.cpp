#define LOG_TAG "AudioTrackDemo"

#include <utils/Log.h>
#include <media/AudioTrack.h>
#include <binder/MemoryDealer.h>


using namespace android;

//#define TRANSFER_TYPE 0
//0: static buffer mode, 1: streaming mode

char *file_err = "file cannot open!";
char *fmt_err = "wav fmt error!";
char *param_err = "param error!";
char *play_err = "play exception!";
char *play_ok = "play finished!";

struct WaveHeader{
    int riff_id;
    int riff_sz;
    int riff_fmt;
    int fmt_id;
    int fmt_sz;
    short audio_fmt;
    short num_chn;
    int sample_rate;
    int byte_rate;
    short block_align;
    short bits_per_sample;
    int data_id;
    int data_sz;
};

struct at_param {
    char *path;
    int stream_type;
    int transfer_type;  // 0/1/2/3/4: default/callback/obtain/sync/shared
};

struct play_conf {
    FILE *fp;
    struct WaveHeader *header;
    int stream_type;
};

void play_callback(int event, void *user, void *info)
{
    AudioTrack::Buffer *buf;
    struct play_conf *conf = (struct play_conf *)user;

    switch (event) {
    case AudioTrack::EVENT_MORE_DATA:
        buf = (AudioTrack::Buffer *)info;
        ALOGD("onMoreData. expect_size:%d", buf->size);
        fread(buf->i8, 1, buf->size, conf->fp);
        break;
    case AudioTrack::EVENT_UNDERRUN:
        ALOGW("onUnderRun.");
        break;
    case AudioTrack::EVENT_LOOP_END:
        ALOGW("onLoopEnd.");
        break;
    case AudioTrack::EVENT_MARKER:
        ALOGW("onMARKER.");
        break;
    case AudioTrack::EVENT_NEW_POS:
        ALOGW("onNewPos.");
        break;
    case AudioTrack::EVENT_BUFFER_END:
        ALOGW("onBufEnd.");
        break;
    case AudioTrack::EVENT_NEW_IAUDIOTRACK:
        ALOGW("onNewIAudioTrack.");
        break;
    case AudioTrack::EVENT_STREAM_END:
        ALOGW("onStreamEnd.");
        break;
    case AudioTrack::EVENT_NEW_TIMESTAMP:
        ALOGW("onNewTimestamp.");
        break;
    default:
        ALOGE("unknow event(%d)!", event);
        break;
    }
}

char* play_transfer_callback(struct play_conf *conf)
{
    char *ret_info = play_ok;
    int mini_frm_sz;

    ALOGD("transfer_type: CALLBACK!");

    struct WaveHeader *pHeader = conf->header;
    int pcm_sz = pHeader->data_sz;
    int dura_sec = pcm_sz / (pHeader->sample_rate * pHeader->num_chn * sizeof(short));

    sp<AudioTrack> sp_at = new AudioTrack((audio_stream_type_t)conf->stream_type,
        pHeader->sample_rate,
        AUDIO_FORMAT_PCM_16_BIT,
        audio_channel_out_mask_from_count(pHeader->num_chn),
        0,
        AUDIO_OUTPUT_FLAG_NONE,
        play_callback,
        conf,
        0,
        0,
        AudioTrack::TRANSFER_DEFAULT,
        NULL,
        -1);

    size_t frame_cnt;
    AudioTrack::getMinFrameCount(&frame_cnt, AUDIO_STREAM_MUSIC, pHeader->sample_rate);
    ALOGD("minFrameCount=%d for sr(%d), chn(%d)", frame_cnt, pHeader->sample_rate, pHeader->num_chn);
    ALOGD("session_id=%d, play will continue %d seconds!", sp_at->getSessionId(), dura_sec);

    status_t status = sp_at->initCheck();
    if(status != NO_ERROR) {
        ALOGE("Failed for initCheck()! status=%#x", status);
        ret_info = play_err;
        goto PLAY_EXIT;
    }

    sp_at->start();

    sleep(dura_sec+1);

    sp_at->stop();

PLAY_EXIT:
    sp_at.clear();

    return ret_info;
}

char* play_transfer_obtain(struct play_conf *conf)
{
    char *ret_info = play_ok;
    int mini_frm_sz;

    ALOGD("transfer_type: OBTAIN! Deprecated API !!!");

    struct WaveHeader *pHeader = conf->header;
    int pcm_sz = pHeader->data_sz;
    int dura_sec = pcm_sz / (pHeader->sample_rate * pHeader->num_chn * sizeof(short));

    sp<AudioTrack> sp_at = new AudioTrack((audio_stream_type_t)conf->stream_type,
        pHeader->sample_rate,
        AUDIO_FORMAT_PCM_16_BIT,
        audio_channel_out_mask_from_count(pHeader->num_chn),
        0,
        AUDIO_OUTPUT_FLAG_NONE,
        NULL,
        NULL,
        0,
        0,
        AudioTrack::TRANSFER_OBTAIN,
        NULL,
        -1);

    size_t frame_cnt;
    AudioTrack::getMinFrameCount(&frame_cnt, AUDIO_STREAM_MUSIC, pHeader->sample_rate);
    ALOGD("minFrameCount=%d for sr(%d), chn(%d)", frame_cnt, pHeader->sample_rate, pHeader->num_chn);
    ALOGD("session_id=%d, play will continue %d seconds!", sp_at->getSessionId(), dura_sec);

    status_t status = sp_at->initCheck();
    if(status != NO_ERROR) {
        ALOGE("Failed for initCheck()! status=%#x", status);
        ret_info = play_err;
        goto PLAY_EXIT;
    }

    sp_at->start();

    mini_frm_sz = frame_cnt*2*pHeader->num_chn;
    while (1) {
        AudioTrack::Buffer buffer;
        //buffer.frameCount = 4096;
        buffer.frameCount = frame_cnt;

        status_t status = sp_at->obtainBuffer(&buffer, -1);
        if (status == NO_ERROR) {
            int rd_sz = fread(buffer.i8, 1, buffer.size, conf->fp);
            sp_at->releaseBuffer(&buffer);
            if (rd_sz != buffer.size) {
                ALOGW("file found EOF! rd_sz[%d] != buffer.size[%d]", rd_sz, buffer.size);
                break;
            }
        } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
            ALOGE("can NOT write to AudioTrack! status[%#x]", status);
            break;
        }
    }
    sp_at->stop();

PLAY_EXIT:
    sp_at.clear();

    return ret_info;
}

char* play_transfer_sync(struct play_conf *conf)
{
    char *ret_info = play_ok;
    int mini_frm_sz;
    void *buf;
    int rd_sz;

    ALOGD("transfer_type: SYNC");

    struct WaveHeader *pHeader = conf->header;
    int pcm_sz = pHeader->data_sz;
    int dura_sec = pcm_sz / (pHeader->sample_rate * pHeader->num_chn * sizeof(short));

    sp<AudioTrack> sp_at = new AudioTrack((audio_stream_type_t)conf->stream_type,
        pHeader->sample_rate,
        AUDIO_FORMAT_PCM_16_BIT,
        audio_channel_out_mask_from_count(pHeader->num_chn),
        0);

    size_t frame_cnt;
    AudioTrack::getMinFrameCount(&frame_cnt, AUDIO_STREAM_MUSIC, pHeader->sample_rate);
    ALOGD("minFrameCount=%d for sr(%d), chn(%d)", frame_cnt, pHeader->sample_rate, pHeader->num_chn);
    ALOGD("session_id=%d, play will continue %d seconds!", sp_at->getSessionId(), dura_sec);

    status_t status = sp_at->initCheck();
    if(status != NO_ERROR) {
        ALOGE("Failed for initCheck()! status=%#x", status);
        ret_info = play_err;
        goto PLAY_EXIT;
    }

    sp_at->start();

    mini_frm_sz = frame_cnt*2*pHeader->num_chn;
    buf = malloc(mini_frm_sz);
    while ((rd_sz = fread(buf, 1, mini_frm_sz, conf->fp)) > 0) {
        sp_at->write(buf, rd_sz);
    }
    sp_at->stop();
    free(buf);

PLAY_EXIT:
    sp_at.clear();

    return ret_info;
}

char* play_transfer_shared(struct play_conf *conf)
{
    sp<MemoryDealer> heap;
    sp<IMemory> iMem;
    uint16_t *mem;
    //int dura_sec = 15;
    char *ret_info;

    ALOGD("transfer_type: SHARED");

    struct WaveHeader *pHeader = conf->header;
    int pcm_sz = pHeader->data_sz;
    int dura_sec = pcm_sz / (pHeader->sample_rate * pHeader->num_chn * sizeof(short));
    //int pcm_sz = dura_sec * pHeader->sample_rate * pHeader->num_chn * sizeof(short);

    heap = new MemoryDealer(pcm_sz, "AudioTrackDemo Heap Base");
    iMem = heap->allocate(pcm_sz);
    mem = static_cast<uint16_t*>(iMem->pointer());
    fread(mem, 1, pcm_sz, conf->fp);

    sp<AudioTrack> sp_at = new AudioTrack((audio_stream_type_t)conf->stream_type,
        pHeader->sample_rate,
        AUDIO_FORMAT_PCM_16_BIT,
        audio_channel_out_mask_from_count(pHeader->num_chn),
        iMem);

    size_t frame_cnt;
    AudioTrack::getMinFrameCount(&frame_cnt, AUDIO_STREAM_MUSIC, pHeader->sample_rate);
    ALOGD("minFrameCount=%d for sr(%d), chn(%d)", frame_cnt, pHeader->sample_rate, pHeader->num_chn);
    ALOGD("session_id=%d, play will continue %d seconds!", sp_at->getSessionId(), dura_sec);

    status_t status = sp_at->initCheck();
    if(status != NO_ERROR) {
        ALOGE("Failed for initCheck()! status=%#x", status);
        ret_info = play_err;
        goto PLAY_EXIT;
    }
    sp_at->start();
    sleep(dura_sec);
    sp_at->stop();
    ret_info = play_ok;

PLAY_EXIT:
    iMem.clear();
    heap.clear();
    sp_at.clear();

    return ret_info;
}

void* play_thread(void* argv)
{
    char *ret_info = play_err;

    struct at_param *param = (struct at_param*)argv;
    char *path = param->path;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return file_err;
    }
    struct WaveHeader header;
    int rd_cnt = fread(&header, sizeof(header), 1, fp);

    if ((header.riff_id!='FFIR') || (header.riff_fmt!='EVAW') || (header.fmt_id != ' tmf')) {
        ALOGE("wav format detect error! riff_id:%#x, riff_fmt:%#x, fmt_id:%#x", header.riff_id, header.riff_fmt, header.fmt_id);
        fclose(fp);
        return fmt_err;
    }

    struct play_conf conf;
    conf.fp = fp;
    conf.header = &header;
    conf.stream_type = param->stream_type;

    switch (param->transfer_type) {
    case AudioTrack::TRANSFER_DEFAULT:
        ALOGW("DEFAULT mode use SYNC");
        ret_info = play_transfer_sync(&conf);
        break;

    case AudioTrack::TRANSFER_CALLBACK:
        ret_info = play_transfer_callback(&conf);
        break;

    case AudioTrack::TRANSFER_OBTAIN:
        //deprecated API
        ret_info = play_transfer_obtain(&conf);
        break;

    case AudioTrack::TRANSFER_SYNC:
        ret_info = play_transfer_sync(&conf);
        break;

    case AudioTrack::TRANSFER_SHARED:
        ret_info = play_transfer_shared(&conf);
        break;

    default:
        ALOGW("unknown transfer_type(%d)", param->transfer_type);
        ret_info = param_err;
        break;
    }

    fclose(fp);

    return ret_info;
}

int main(int argc, char** argv)
{
    ALOGD("---------AudioTrackDemo begin----------");
    ALOGD("function: test pcm playback");

    if (argc != 4) {
        ALOGE("argc(%d) must be 4, argv format: filepath + stream_type + transfer_type)", argc);
        ALOGI("Base on Android4.4...");
        ALOGI("more info, stream_type:   -1-default; 0-voice_call; 1-system; 2-ring; 3-music; 4-alarm; 5-notification; 6-bluetooth_sco; 7-enforced; 8-dtmf; 9-tts");
        ALOGI("           transfer_type:  0-default; 1-call_back;  2-obtain; 3-sync; 4-shared");
        return -1;
    }

    struct at_param param;
    param.path = argv[1];
    param.stream_type = atoi(argv[2]);
    param.transfer_type = atoi(argv[3]);

    pthread_t tid;
    pthread_create(&tid, NULL, play_thread, &param);

    ALOGD("join tid begin...");
    void *retval;
    pthread_join(tid, &retval);
    ALOGD("join tid end!!! ret_val(%s)", (char*)retval);

    ALOGD("---------AudioTrackDemo end----------");

    return 0;
}
