#define LOG_TAG "AudioRecordDemo"

#include <utils/Log.h>
#include <media/AudioRecord.h>

#define CAP_COUNT 1

#define CAP_CALLBACK

using namespace android;

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

void audio_fix_wavheader(struct WaveHeader *pHeader, int sampleRate, int chnCnt, int pcmSize)
{
    memcpy(&pHeader->riff_id, "RIFF", 4);
    pHeader->riff_sz = pcmSize + sizeof(struct WaveHeader) - 8;
    memcpy(&pHeader->riff_fmt, "WAVE", 4);
    memcpy(&pHeader->fmt_id, "fmt ", 4);
    pHeader->fmt_sz = 16;
    pHeader->audio_fmt = 1;       // s16le
    pHeader->num_chn = chnCnt;
    pHeader->sample_rate = sampleRate;
    pHeader->byte_rate = sampleRate * chnCnt * 2;
    pHeader->block_align = chnCnt * 2;
    pHeader->bits_per_sample = 16;
    memcpy(&pHeader->data_id, "data", 4);
    pHeader->data_sz = pcmSize;
}

struct AR_PARAM {
    audio_source_t src;
    int sr;
    int chn;
    int dura_s;
    char path[32];
};

void pcm_cb(int event, void *user, void *info)
{
    switch (event) {
        case AudioRecord::EVENT_MORE_DATA: {
            AudioRecord::Buffer* buf = (AudioRecord::Buffer*)info;
            int sz = buf->size;
            short *data = buf->i16;
            ALOGD("pcm_callback! size:%d", sz);
            fwrite(data, 1, sz, (FILE*)user);
            break;
        }
        case AudioRecord::EVENT_OVERRUN: {
            ALOGW("AudioRecord reported overrun!");
            break;
        }
        default: {
            ALOGE("AudioRecord reported error event:%d !", event);
            break;
        }
    }
}

void* capture_thread(void* argv)
{
    AR_PARAM *param = (AR_PARAM *)argv;
    ALOGD("ready to gen file: %s, with audio param(src:%d, sr:%d, chn:%d)", param->path,param->src,param->sr,param->chn);

    FILE *fp = fopen(param->path, "wb");
    if (!fp) {
        ALOGE("file(%s) can not create?", param->path);
        return NULL;
    } else {
        struct WaveHeader header;
        int pcmLen = param->dura_s * param->chn * param->sr * 2;
        audio_fix_wavheader(&header, param->sr, param->chn, pcmLen);
        fwrite(&header, 1, sizeof(header), fp);
    }

#ifndef CAP_CALLBACK
    audio_channel_mask_t chn = audio_channel_in_mask_from_count(param->chn);
    sp<AudioRecord> ar = new AudioRecord();
    ar->set((audio_source_t)param->src, param->sr, AUDIO_FORMAT_PCM_16_BIT, chn, 0);
    int ret = ar->initCheck();
    if (ret) {
        ALOGD("initCheck error: %d", ret);
        return NULL;
    }
    ret = ar->start();
    if (ret) {
        ALOGD("start error: %d", ret);
        return NULL;
    }

    int loop_cnt=0;
    int rd_cnt = param->sr * param->chn * 2;
    void *buf = malloc(rd_cnt);

    ALOGW("ready read pcm...");
    while(loop_cnt++ < param->dura_s) {
        ret = ar->read(buf, rd_cnt);
        fwrite(buf, 1, ret, fp);
        ALOGD("loop_cnt: %d, read %d bytes and write to file", loop_cnt, ret);
    }

    ar->stop();
    fclose(fp);

    ar.clear();
#else
    audio_channel_mask_t chn = audio_channel_in_mask_from_count(param->chn);
    size_t minFrmCnt;
    status_t status = AudioRecord::getMinFrameCount(&minFrmCnt, param->sr, AUDIO_FORMAT_PCM_16_BIT, chn);
    sp<AudioRecord> sp_ar = new AudioRecord(
                                    (audio_source_t)param->src, param->sr, AUDIO_FORMAT_PCM_16_BIT, chn,
                                    0,
                                    pcm_cb,
                                    fp,
                                    0
                                    );
    status = sp_ar->initCheck();
    ALOGD("init_check:%d", status);
    sp_ar->start();
    sleep(param->dura_s);
    sp_ar->stop();
    fclose(fp);

    sp_ar.clear();
#endif

    ALOGD("----------------thread exit----------------");
    return NULL;
}


int main(int argc, char** argv)
{
    ALOGD("---------AudioRecordDemo begin----------");
    ALOGD("function: test AUDIO_SOURCE(MIC, SPK or REF) capture");

    if (argc != 1+CAP_COUNT*5)
    {
        ALOGE("argc(%d) should be(%d)! argv should be: AudioRecordDemo MIC/REF/OTHER(src,samplerate,chn,dura_s,filename)X%d", argc, 1+CAP_COUNT*5, CAP_COUNT);
        ALOGE("src: 0-mic, 10-spk_ref, use this for Cap Source!!!");
        return -1;
    }

    struct AR_PARAM mic_para, ref_para, other_para;

    mic_para.src = (audio_source_t)atoi(argv[1]);
    mic_para.sr = atoi(argv[2]);
    mic_para.chn = atoi(argv[3]);
    mic_para.dura_s = atoi(argv[4]);
    strncpy(mic_para.path, argv[5], 32);

    #if CAP_COUNT >= 2
    ref_para.src = (audio_source_t)atoi(argv[6]);
    ref_para.sr = atoi(argv[7]);
    ref_para.chn = atoi(argv[8]);
    ref_para.dura_s = atoi(argv[9]);
    strncpy(ref_para.path, argv[10], 32);
    #if CAP_COUNT == 3
    other_para.src = (audio_source_t)atoi(argv[11]);
    other_para.sr = atoi(argv[12]);
    other_para.chn = atoi(argv[13]);
    other_para.dura_s = atoi(argv[14]);
    strncpy(other_para.path, argv[15], 32);
    #endif
    #endif

    ALOGD("MIC param: audio_src_t(%d), src(%d), chn(%d), dura(%d s), file(%s)", mic_para.src,mic_para.sr,mic_para.chn,mic_para.dura_s,mic_para.path);
    #if CAP_COUNT >= 2
    ALOGD("REF param: audio_src_t(%d), src(%d), chn(%d), dura(%d s), file(%s)", ref_para.src,ref_para.sr,ref_para.chn,ref_para.dura_s,ref_para.path);
    #if CAP_COUNT == 3
    ALOGD("OTHER param: audio_src_t(%d), src(%d), chn(%d), dura(%d s), file(%s)", other_para.src,other_para.sr,other_para.chn,other_para.dura_s,other_para.path);
    #endif
    #endif

    pthread_t tid1, tid2, tid3;

    pthread_create(&tid1, NULL, capture_thread, &mic_para);
    #if CAP_COUNT >= 2
    pthread_create(&tid2, NULL, capture_thread, &ref_para);
    #if CAP_COUNT == 3
    pthread_create(&tid3, NULL, capture_thread, &other_para);
    #endif
    #endif

    ALOGD("wait for join tid1/2 and tid3 thread begin...");
    pthread_join(tid1, NULL);
    #if CAP_COUNT >= 2
    pthread_join(tid2, NULL);
    #if CAP_COUNT == 3
    pthread_join(tid3, NULL);
    #endif
    #endif
    ALOGD("wait for join tid1/2 and tid3 thread end!!!");

    ALOGD("---------AudioRecordDemo end----------");

    return 0;
}
