#define LOG_TAG "TestBinderService"

#include <utils/Log.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>

#include "TestBinderService.h"

static int debug_flag = 1;

namespace android {

void TestBinderService::instantiate() {
    ALOGI("Enter TestBinderService::instantiate");
    status_t st = defaultServiceManager()->addService(String16("zjz.tools.TestBinder"), new TestBinderService());
    ALOGD("ServiceManager addService ret=%d", st);
}

TestBinderService::TestBinderService() {
    ALOGI("TestBinderService ctor");
}

TestBinderService::~TestBinderService() {
    ALOGI("TestBinderService dtor");
}

int TestBinderService::add(int a, int b) {
    ALOGD("TestBinderService::add a = %d, b = %d.", a , b);
    return a+b;
}

int TestBinderService::share_mem(const sp<IMemory> &mem) {
    ALOGD("TestBinderService::share_mem dumpInfo(get:%p, pointer:%p, size:%d).", mem.get(), mem->pointer(), mem->size());
    return 0;
}

}
