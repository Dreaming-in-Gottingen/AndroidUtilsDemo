#define LOG_TAG "TestBinderClient"

#include <utils/Log.h>
#include <utils/String8.h>

#include <binder/MemoryDealer.h>
#include <binder/IServiceManager.h>

#include "../TestBinderServer/ITestBinderService.h"

using namespace android;

int main(int argc, char** argv)
{
    int sum = 0;
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder;
    const char *srv_name="zjz.tools.TestBinder";
    do {
        binder = sm->getService(String16(srv_name));
        if (binder != 0)
            break;
        ALOGI("getService(%s) fail! sleep 1s then continue...", srv_name);
        sleep(1);
    } while (true);

    sp<ITestBinderService> srv = interface_cast<ITestBinderService>(binder);
    ALOGE_IF(srv == 0, "no ITestBinserService!?");

    sum = srv->add(3, 4);
    ALOGI("sum = %d", sum);

    sp<MemoryDealer> heap = new MemoryDealer(1024*1024, "MemoryDealerDemo");
    sp<IMemory> iMem = heap->allocate(512*1024);
    char *ptr = static_cast<char*>(iMem->pointer());
    strcpy(ptr, "MemoryDealerDemo");
    strcpy(ptr+511*1024, "MemoryDealerDemo end");

    int ret = srv->share_mem(iMem);
    ALOGE_IF(ret != 1, "client call service::share_mem, ret=%d", ret);

    return 0;
}
