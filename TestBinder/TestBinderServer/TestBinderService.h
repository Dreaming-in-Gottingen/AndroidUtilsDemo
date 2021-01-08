#ifndef ANDROID_TESTBINDERSERVICE_H_
#define ANDROID_TESTBINDERSERVICE_H_

#include <utils/KeyedVector.h>
#include "ITestBinderService.h"

namespace android {

class TestBinderService: public BnTestBinderService {
public:
    static void instantiate();
    int add(int a, int b);
    int share_mem(const sp<IMemory> &iMem);

private:
    TestBinderService();
    virtual ~TestBinderService();
};

}

#endif /* ANDROID_TESTBINDERSERVICE_H_ */
