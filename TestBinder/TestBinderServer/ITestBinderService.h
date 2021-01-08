#ifndef ANDROID_ITESTBINDERSERVICE_H_
#define ANDROID_ITESTBINDERSERVICE_H_

#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

#include <binder/IMemory.h>
#include <binder/MemoryHeapBase.h>

namespace android {

class ITestBinderService: public IInterface {
public:
    DECLARE_META_INTERFACE(TestBinderService);

    virtual int add(int a, int b) = 0;
    virtual int share_mem(const sp<IMemory> &mem) = 0;
};

class BnTestBinderService: public BnInterface<ITestBinderService> {
public:
    virtual status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0);
};

}

#endif /* ANDROID_ITESTBINDERSERVICE_H_ */
