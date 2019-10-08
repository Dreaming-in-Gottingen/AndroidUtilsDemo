#define LOG_NDEBUG 0
#define LOG_TAG "ALooperTester"
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ABuffer.h>
//#include <media/stagefright/foundation/ADebug.h>

namespace android {

struct Obj : public RefBase {
    Obj() {
        ALOGD("Obj ctor!");
    }
    ~Obj() {
        ALOGD("Obj dtor!");
    }

    void setName(AString str) {
        name = str;
    }
    AString getName() {
        return name;
    }

    void onFirstRef() {
        ALOGW("Obj onFirstRef!");
    }

    void onLastStrongRef(const void*) {
        ALOGW("Obj onLastStrongRef!");
    }

    void onLastWeakRef(const void*) {
        ALOGW("Obj onLastWeakRef!");
    }

private:
    AString name;
};

//class ALooperTester : public AHandler {
//public:
struct ALooperTester : public AHandler {
    ALooperTester();
    void start();
    void read();
    void wait();
    void other();
    void stop();

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg);
    ~ALooperTester();

private:
    enum {
        kWhatStart  = 'strt',
        kWhatRead   = 'read',
        kWhatWait   = 'wait',
        kWhatOther  = 'othr',
        kWhatStop   = 'stop',
        kWhatNewMsg = 'mesg',
    };

    sp<ALooper> mLooper;
    sp<AMessage> mNotify;
};

ALooperTester::ALooperTester()
{
    mLooper = new ALooper();
    mLooper->setName("ALooperTester");
    ALOGD("ALooperTester ctor");
}

ALooperTester::~ALooperTester()
{
    mLooper->unregisterHandler(id());
    mLooper->stop();
    ALOGD("ALooperTester dtor");
}

void ALooperTester::start()
{
    //registerHandler(this) CAN NOT be done in ctor, because 'this' obj is constructing.
    mLooper->registerHandler(this);
    mLooper->start();

    ALOGD("--------start begin----------");
    //deliver may fail when mLooper's internal thread has not run.
    sp<AMessage> msg = new AMessage(kWhatStart, id());
    msg->post();
    ALOGD("--------start end----------");
}

void ALooperTester::read()
{
    ALOGD("--------read begin--------");
    sp<AMessage> msg = new AMessage(kWhatRead, id());
    msg->post(2000*1000);
    ALOGD("--------read end---test delay(2s) post-------");
}

void ALooperTester::wait()
{
    ALOGD("--------wait begin--------");
    sp<AMessage> msg = new AMessage(kWhatWait, id());
    msg->setInt32("val", 123456);

    sp<AMessage> response;
    msg->postAndAwaitResponse(&response);
    ALOGD("--------wait end----------");
    ALOGD("dump response info:");
    ALOGD("%s", response->debugString().c_str());
}

void ALooperTester::other()
{
    ALOGD("--------other(Object,Buffer,Message,Rect) begin--------");
    sp<AMessage> msg = new AMessage(kWhatOther, id());

    //Object
    sp<Obj> obj = new Obj();
    obj->setName("This is Obj.");
    msg->setObject("Object", obj);

    //ABuffer
    sp<ABuffer> abuf = new ABuffer(64);
    char *buf = "This is ABuffer.";
    memcpy(abuf->data(), buf, strlen(buf));
    abuf->setRange(0, strlen(buf));
    msg->setBuffer("abuf", abuf);

    //AMessage
    sp<AMessage> mm = new AMessage(kWhatNewMsg, id());
    mm->setBuffer("buffer", abuf);
    msg->setMessage("message", mm);
    //msg->setMessage("message", msg); //CAN NOT transfer itself!!!

    //Rect
    msg->setRect("rect", 0, 0, 1920, 1080);

    msg->post();
    ALOGD("--------other(Object,Buffer,Message,Rect) end--------");
}

void ALooperTester::stop()
{
    ALOGD("--------stop begin-------");
    sp<AMessage> msg = new AMessage(kWhatStop, id());
    msg->post();
    // after unregisterHandler, msg->post will cause ALooperRoster do not know where to post
    //mLooper->unregisterHandler(id());
    //mLooper->stop();
    ALOGD("--------stop end----------");
}

void ALooperTester::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatStart:
        {
            ALOGD("onMsg. start!");
            break;
        }
        case kWhatRead:
        {
            ALOGD("onMsg. delay read!");
            break;
        }
        case kWhatWait:
        {
            ALOGD("onMsg. wait! need response to sender!");
            int32_t v;
            bool ret = msg->findInt32("val", &v);
            if (ret) {
                ALOGI("find 'val': %d", v);
            } else {
                ALOGW("not find 'val'!");
            }
            uint32_t replyID;
            msg->senderAwaitsResponse(&replyID);
            msg->setString("replyInfo", "hello,world", 64);
            msg->postReply(replyID);
            break;
        }
        case kWhatOther:
        {
            ALOGD("onMsg. other!");
            ALOGD("dump Other...");
            ALOGD("-----------------------------------------------------------------");
            ALOGD("%s", msg->debugString().c_str());
            ALOGD("-----------------------------------------------------------------");

            //Object
            sp<RefBase> o;
            char *name = "Object";
            bool ret = msg->findObject(name, &o);
            sp<Obj> oo = static_cast<Obj*>(o.get());
            //Obj *oo = static_cast<Obj*>(o.get());
            if (ret) {
                ALOGD("find '%s'! content: '%s', addr:%p, ref:%d", name, oo->getName().c_str(), oo.get(), oo->getStrongCount());
            } else {
                ALOGD("not find '%s'!", name);
            }

            //ABuffer
            sp<ABuffer> buf;
            name = "abuf";
            ret = msg->findBuffer(name, &buf);
            if (ret) {
                ALOGD("find '%s'! content_sz:%d", name, buf->size());
            } else {
                ALOGD("not find '%s'!", name);
            }

            //AMessage
            name = "message";
            sp<AMessage> mm;
            ret = msg->findMessage(name, &mm);
            if (ret) {
                ALOGD("find '%s'! send it out!", name);
                mm->post(); //next msg will go to default when NO case_kWhatNewMsg!
            } else {
                ALOGD("not find '%s'!", name);
            }

            //Rect
            name = "rect";
            int32_t l,t,r,b;
            ret = msg->findRect(name, &l, &t, &r, &b);
            if (ret) {
                ALOGD("find '%s'! l:%d, t:%d, r:%d, b:%d", name, l, t, r, b);
            } else {
                ALOGD("not find '%s'!", name);
            }

            break;
        }
        case kWhatStop:
        {
            ALOGD("onMsg. stop!");
            break;
        }
        case kWhatNewMsg:
        {
            ALOGD("onMsg. NewMsg!");
            ALOGD("dump NewMsg...");
            ALOGD("-----------------------------------------------------------------");
            ALOGD("%s", msg->debugString().c_str());
            ALOGD("-----------------------------------------------------------------");
            break;
        }
        default:
        {
            ALOGD("unknown msg! what:%#x", msg->what());
            ALOGD("dump msg: %s", msg->debugString().c_str());
            break;
        }
    }
}

}; //ALooperTester

using namespace android;
int main(void)
{
    ALOGD(">>>>>>>>>>main begin<<<<<<<<<<");
// below for testing sp/wp
//    sp<Obj> sp_obj = new Obj();
//    sp<Obj> sp_obj1 = sp_obj;
//    sp_obj = NULL;
//    //sp_obj1.get()->incStrong(sp_obj1.get());
//    //sp_obj1 = NULL;
//    //sp_obj1.clear();
//    wp<Obj> wp_obj = sp_obj1;
//    wp<Obj> wp_obj1 = wp_obj;
//    sp<Obj> tmp = wp_obj.promote();
//    if (tmp != NULL) {
//        tmp->setName("jingzhou.zhang");
//        ALOGD("name: %s", tmp->getName().c_str());
//        int wcnt = tmp->getWeakRefs()->getWeakCount();
//        int scnt = tmp->getStrongCount();
//        ALOGD("weak_cnt:%d, strong_cnt:%d\n", wcnt, scnt);
//    } else {
//        ALOGD("promote wp fail!");
//    }

    sp<ALooperTester> tester = new ALooperTester();
    tester->start();
    ALOGD("start->read...");
    tester->read();
    ALOGD("read->wait...");
    tester->wait();
    ALOGD("wait->other...");
    tester->other();
    ALOGD("other->stop...");
    tester->stop();
    //tester->read(); //msg deliver fail after unregisterHandler!!!
    sleep(3);
    tester.clear(); //equal to tester=NULL;
    ALOGD(">>>>>>>>>>>main end<<<<<<<<<<<");

    return 0;
}
