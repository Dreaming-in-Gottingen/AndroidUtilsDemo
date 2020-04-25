//#define LOG_NDEBUG 0
#define LOG_TAG "RefBaseTester"

#include <utils/Log.h>
#include <utils/RefBase.h>

using namespace android;

class DerivedTest : public RefBase
{
public:
    DerivedTest();

protected:
    void onFirstRef();
    void onLastStrongRef();
    void onLastStrongRef(const void* id);

private:
    ~DerivedTest();
};

DerivedTest::DerivedTest()
{
    ALOGD("DerivedTest ctor! this[%p]", this);
}

DerivedTest::~DerivedTest()
{
    ALOGD("DerivedTest dtor! this[%p]", this);
}

void DerivedTest::onFirstRef()
{
    ALOGD("DerivedTest onFirstRef");
}

void DerivedTest::onLastStrongRef()
{
    ALOGD("DerivedTest onLastStrongRef");
}

void DerivedTest::onLastStrongRef(const void* id)
{
    ALOGD("DerivedTest onLastStrongRef with param[%p]", id);
}

// ------------------------------------------------------------------------
void testAutoRelease()
{
    ALOGD("--------------");
    ALOGD("function:%s", __FUNCTION__);
    sp<DerivedTest> sp_dt = new DerivedTest();
    ALOGD("--------------");
}

void testHandRelease()
{
    ALOGD("--------------");
    ALOGD("function:%s", __FUNCTION__);
    sp<DerivedTest> sp_dt = new DerivedTest();
    sp_dt->decStrong(sp_dt.get());
    sp_dt.clear();
    //sp_dt = NULL;   // equal to clear()
    ALOGD("--------------");
}

void testRefCounter()
{
    ALOGD("--------------");
    ALOGD("function:%s", __FUNCTION__);
    DerivedTest *pDT = new DerivedTest();
    sp<DerivedTest> sp_dt;
    ALOGD("0000. INITIAL_STRONG_VALUE==pDT->getStrongCount(): [%#x]", pDT->getStrongCount());
    ALOGD("0000. sp_dt.get(): [%p], &sp_dt: [%p]", sp_dt.get(), &sp_dt);
    //ALOGD("1111. strongCounter: [%d], crash!", sp_dt->getStrongCount());
    sp_dt = pDT;
    ALOGD("2222. strongCounter: [%d]", sp_dt->getStrongCount());
    ALOGD("2222. weakCounter: [%d]", sp_dt->getWeakRefs()->getWeakCount());
    sp_dt->incStrong(NULL);
    ALOGD("3333. after incStrong(), strongCounter: [%d]", sp_dt->getStrongCount());
    ALOGD("3333. after incStrong(), weakCounter: [%d]", sp_dt->getWeakRefs()->getWeakCount());
    sp_dt->decStrong(NULL);
    //sp_dt.clear();
    ALOGD("--------------");
}

void testSwapObj()
{
    ALOGD("--------------");
    ALOGD("function:%s", __FUNCTION__);
    sp<DerivedTest> sp_dt0 = new DerivedTest();
    sp<DerivedTest> sp_dt1 = new DerivedTest();
    ALOGD("0000. &sp_dt0:[%p], sp_dt0.get():[%p], &sp_dt1:[%p], sp_dt1.get():[%p]",
            &sp_dt0, sp_dt0.get(), &sp_dt1, sp_dt1.get());
    sp_dt0 = sp_dt1;
    ALOGD("1111. &sp_dt0:[%p], sp_dt0.get():[%p], &sp_dt1:[%p], sp_dt1.get():[%p]",
            &sp_dt0, sp_dt0.get(), &sp_dt1, sp_dt1.get());
    ALOGD("sp_dt0 info: strongCounter:[%d], weakCounter:[%d]",
            sp_dt0->getStrongCount(), sp_dt0->getWeakRefs()->getWeakCount());
    ALOGD("sp_dt1 info: strongCounter:[%d], weakCounter:[%d]",
            sp_dt1->getStrongCount(), sp_dt1->getWeakRefs()->getWeakCount());
    ALOGD("--------------");
}

int main()
{
    ALOGW("-----------RefBaseTester begin-------------");

    testAutoRelease();
    testHandRelease();
    testRefCounter();
    testSwapObj();

    ALOGW("-----------RefBaseTester end-------------");

    return 0;
}
