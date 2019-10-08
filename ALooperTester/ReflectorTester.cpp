#define LOG_NDEBUG 0
#define LOG_TAG "ReflectorTester"
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

struct TrackWorker: public AHandler {
    TrackWorker() {
        ALOGI("TrackWorkder ctor!");
    };

    void init(sp<AMessage> &msg);
    void start();
    void read();
    void stop();

protected:
    ~TrackWorker() {
        ALOGI("TrackWorkder dtor!");
    };

    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum {
        kWhatWorkerInit  = 'init',
        kWhatWorkerStart = 'strt',
        kWhatWorkerRead  = 'read',
        kWhatWorkerStop  = 'stop',
    };

    int mReadCnt;
    sp<ALooper> mLooper;
    sp<AMessage> mNotify;
};

void TrackWorker::init(sp<AMessage> &msg)
{
    mReadCnt = 0;
    mNotify = msg;

    mLooper = new ALooper();
    mLooper->setName("TrackWorker");
    mLooper->registerHandler(this);
    mLooper->start();
}

void TrackWorker::start()
{
    (new AMessage(kWhatWorkerStart, id()))->post();
}

//called by HandlerRlefectorTester::onMessage
void TrackWorker::read()
{
    (new AMessage(kWhatWorkerRead, id()))->post();
}

void TrackWorker::stop()
{
    (new AMessage(kWhatWorkerStop, id()))->post();
    sleep(1);
    mLooper->unregisterHandler(id());
    mLooper->stop();
}

void TrackWorker::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatWorkerStart:
        {
            ALOGD("Worker::onMsg. start!");
            (new AMessage(kWhatWorkerRead, id()))->post();
            break;
        }
        case kWhatWorkerRead:
        {
            ALOGD("Worker::onMsg(%dth). read and trigger write repeatly!", mReadCnt++);
            sleep(1);
            mNotify->post();
            break;
        }
        case kWhatWorkerStop:
        {
            ALOGD("Worker::onMsg. stop! NOT Read and Write any more!");
            break;
        }
        default:
        {
            ALOGD("Worker::onMsg. error -> what:%d", msg->what());
            break;
        }
    }
}


struct HandlerReflectorTester : public RefBase {
//class HandlerReflectorTester : public RefBase {
//public:
    HandlerReflectorTester();
    void init();
    void start();
    void write();   //not use
    void reset();
    void stop() {reset();}

    virtual void onMessageReceived(const sp<AMessage> &msg);

protected:
    ~HandlerReflectorTester();

private:
    enum {
        kWhatStart = 'strt',
        kWhatWrite = 'writ',
        kWhatStop  = 'stop',
    };

    int mWriteCnt;
    sp<AHandlerReflector<HandlerReflectorTester> > mReflector;
    sp<ALooper> mLooper;
    sp<AMessage> mNotify;
    sp<TrackWorker> mWorker;
};

HandlerReflectorTester::HandlerReflectorTester()
{
    ALOGD("HandlerReflectorTester ctor");
}

HandlerReflectorTester::~HandlerReflectorTester()
{
    mLooper->unregisterHandler(mReflector->id());
    mLooper->stop();
    ALOGD("HandlerReflectorTester dtor");
}

void HandlerReflectorTester::init()
{
    mWriteCnt = 0;
    mLooper = new ALooper();
    mLooper->setName("ReflectorTester");

    mReflector = new AHandlerReflector<HandlerReflectorTester>(this);
    mLooper->registerHandler(mReflector);
    mLooper->start();

    mNotify = new AMessage(kWhatWrite, mReflector->id());
}

void HandlerReflectorTester::start()
{
    ALOGD("HandlerReflectorTester::start start. init and start worker!");
    mWorker = new TrackWorker();
    mWorker->init(mNotify);
    mWorker->start();

    (new AMessage(kWhatStart, mReflector->id()))->post();
    ALOGD("HandlerReflectorTester::start end");
}

void HandlerReflectorTester::write()
{
    ALOGD("--------write begin--------");
    (new AMessage(kWhatWrite, mReflector->id()))->post();
    ALOGD("--------write end----------");
}

void HandlerReflectorTester::reset()
{
    ALOGD("--------reset begin-------");
    mWorker->stop();
    (new AMessage(kWhatStop, mReflector->id()))->post();
    ALOGD("--------reset begin-------");
}

void HandlerReflectorTester::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatStart:
        {
            ALOGD("onMsg. Trigger start!");
            break;
        }
        case kWhatWrite:
        {
            ALOGD("onMsg. write(%dth)! write and triger worker read again!", mWriteCnt++);
            mWorker->read();
            break;
        }
        case kWhatStop:
        {
            ALOGD("onMsg. stop! ready to exit!");
            break;
        }
        default:
        {
            ALOGD("error: what:%d", msg->what());
            break;
        }
    }
}

}; //ReflectorTester


using namespace android;
int main(void)
{
    ALOGD(">>>>>>>>>>main begin<<<<<<<<<<");
    sp<HandlerReflectorTester> test = new HandlerReflectorTester();

    test->init();
    test->start();
    //test->write();

    sleep(10);

    ALOGD("main: start->stop");
    test->stop();

    sleep(1);

    test.clear();

    ALOGD(">>>>>>>>>>>main end<<<<<<<<<<<");
    return 0;
}
