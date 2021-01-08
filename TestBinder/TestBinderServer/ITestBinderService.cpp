#define LOG_TAG "ITestBinder"

#include <utils/Log.h>

#include "ITestBinderService.h"

namespace android {

enum {
    ADD = IBinder::FIRST_CALL_TRANSACTION,
    SHARE_MEM,
};

class BpTestBinderService: public BpInterface<ITestBinderService>
{
public:
    BpTestBinderService(const sp<IBinder>& impl)
        :BpInterface<ITestBinderService>(impl)
    {
        ALOGI("BpTestBinderService ctor!");
    }

    int add(int a, int b)
    {
        ALOGD("BpTestBinderService:add(%d, %d)", a, b);
        Parcel data, reply;
        data.writeInterfaceToken(ITestBinderService::getInterfaceDescriptor());
        data.writeInt32(a);
        data.writeInt32(b);
        remote()->transact(ADD, data, &reply);
        return reply.readInt32();
    }

    int share_mem(const sp<IMemory> &mem)
    {
        ALOGD("BpTestBinderService:share_mem");
        Parcel data, reply;
        data.writeInterfaceToken(ITestBinderService::getInterfaceDescriptor());
        if (mem != 0) {
            ALOGD("BpTestBinderService:share_mem(get:%p, pointer:%p, size:%d)", mem.get(), mem->pointer(), mem->size());
            data.writeInt32(true);
            data.writeStrongBinder(mem->asBinder());
        } else {
            data.writeInt32(false);
        }
        remote()->transact(SHARE_MEM, data, &reply);
        return reply.readInt32();
    }

};

IMPLEMENT_META_INTERFACE(TestBinderService, "zjz.demo.ITestBinderService");

status_t BnTestBinderService::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code)
    {
        case ADD:
        {
            CHECK_INTERFACE(ITestBinderService, data, reply);
            int a = data.readInt32();
            int b = data.readInt32();
            reply->writeInt32(add(a, b));
            return NO_ERROR;
        }
        case SHARE_MEM:
        {
            CHECK_INTERFACE(ITestBinderService, data, reply);
            bool haveShareMem = data.readInt32() != 0;
            sp<IMemory> iMem;
            if (haveShareMem) {
                iMem = interface_cast<IMemory>(data.readStrongBinder());
                share_mem(iMem);
            } else {
                ALOGD("not use share mem!");
            }
            reply->writeInt32(haveShareMem);
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

}
