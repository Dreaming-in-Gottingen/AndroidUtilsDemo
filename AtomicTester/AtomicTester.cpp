#define LOG_TAG "AtomicTester"
#include <utils/Log.h>

#include <sys/atomics.h>

#include <utils/Atomic.h>

#define USE_ATOMIC_TYPE     1   //1-cutils; 2-bionic; 3-others(mutex)
#define THREAD_COUNT        10
#define ITERATION_COUNT     500000

#define TEST_BIONIC
#ifdef TEST_BIONIC
extern int __atomic_cmpxchg(int old, int _new, volatile int *ptr);
extern int __atomic_swap(int _new, volatile int *ptr);
extern int __atomic_dec(volatile int *ptr);
extern int __atomic_inc(volatile int *ptr);
#endif

static pthread_mutex_t waitLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t waitCond = PTHREAD_COND_INITIALIZER;


#define TEST_MUTEX // not define it will cause wrong result when multi-threads ops the same var.

#ifdef TEST_MUTEX
static pthread_mutex_t incLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t decLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t addLock = PTHREAD_MUTEX_INITIALIZER;
#endif

static volatile int threadsStarted = 0;

/* results */
static int incTest = 0;
static int decTest = 0;
static int addTest = 0;
static int andTest = 0;
static int orTest = 0;
static int casTest = 0;
static int failingCasTest = 0;

/*
 * Get a relative time value.
 */
static int64_t getRelativeTimeNsec()
{
#define HAVE_POSIX_CLOCKS
#ifdef HAVE_POSIX_CLOCKS
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t) now.tv_sec*1000000000LL + now.tv_nsec;
#else
    struct timeval now;
    gettimeofday(&now, NULL);
    return (int64_t) now.tv_sec*1000000000LL + now.tv_usec * 1000LL;
#endif
}


/*
 * Non-atomic implementations, for comparison.
 *
 * If these get inlined the compiler may figure out what we're up to and
 * completely elide the operations.
 */
#define USE_NOINLINE 0
#if USE_NOINLINE == 1
static void incr() __attribute__((noinline));
static void decr() __attribute__((noinline));
static void add(int addVal) __attribute__((noinline));
static int compareAndSwap(int oldVal, int newVal, int* addr) __attribute__((noinline));
#else
static void incr() __attribute__((always_inline));
static void decr() __attribute__((always_inline));
static void add(int addVal) __attribute__((always_inline));
static int compareAndSwap(int oldVal, int newVal, int* addr) __attribute__((always_inline));
#endif

static void incr()
{
#ifdef TEST_MUTEX
    pthread_mutex_lock(&incLock);
#endif
    incTest++;
#ifdef TEST_MUTEX
    pthread_mutex_unlock(&incLock);
#endif
}
static void decr()
{
#ifdef TEST_MUTEX
    pthread_mutex_lock(&decLock);
#endif
    decTest--;
#ifdef TEST_MUTEX
    pthread_mutex_unlock(&decLock);
#endif
}
static void add(int32_t addVal)
{
#ifdef TEST_MUTEX
    pthread_mutex_lock(&addLock);
#endif
    addTest += addVal;
#ifdef TEST_MUTEX
    pthread_mutex_unlock(&addLock);
#endif
}
static int compareAndSwap(int32_t oldVal, int32_t newVal, int32_t* addr)
{
#ifdef TEST_MUTEX
    pthread_mutex_lock(&addLock);
#endif
    if (*addr == oldVal) {
        *addr = newVal;
#ifdef TEST_MUTEX
    pthread_mutex_unlock(&addLock);
#endif
        return 0;
    }
#ifdef TEST_MUTEX
    pthread_mutex_unlock(&addLock);
#endif
    return 1;
}

/*
 * Exercise several of the atomic ops.
 */
static void doAtomicTest(int num)
{
    int addVal = (num & 0x01) + 1;

    int i;
    for (i = 0; i < ITERATION_COUNT; i++) {
        if (USE_ATOMIC_TYPE == 1) {         //cutils, duration=865ms on ARM_CPU_PART_CORTEX_A53
            android_atomic_inc(&incTest); // wrapper for android_atomic_add(1, addr)
            android_atomic_dec(&decTest);
            android_atomic_add(addVal, &addTest);

            int val;
            do {
                val = casTest;
            } while (android_atomic_release_cas(val, val+3, &casTest) != 0);
            //} while (android_atomic_cmpxchg(val, val+3, &casTest) != 0);
            do {
                val = casTest;
            } while (android_atomic_acquire_cas(val, val-1, &casTest) != 0); // same with release_cas
        } else if (USE_ATOMIC_TYPE == 2) {  //bionic, duration=650ms
            __atomic_inc(&incTest); // wrapper for __sync_fetch_and_add(addr, 1), which is gcc built-in API
            __atomic_dec(&decTest);
            __sync_fetch_and_add(&addTest, addVal);

            int val;
            do {
                val = casTest;
            } while (android_atomic_release_cas(val, val+3, &casTest) != 0);
            do {
                val = casTest;
            } while (android_atomic_acquire_cas(val, val-1, &casTest) != 0);
        } else {                            //mutex, noinline_duration=3500ms, inline_duration=28000ms
            //usleep(1); // if no sleep, result may be right; when have sleep, result must be wrong!
            incr();
            decr();
            add(addVal);

            int val;
            do {
                val = casTest;
            } while (compareAndSwap(val, val+3, &casTest) != 0);
            do {
                val = casTest;
            } while (compareAndSwap(val, val-1, &casTest) != 0);
        }
    }
}

/*
 * Entry point for multi-thread test.
 */
static void* atomicTest(void* arg)
{
    pthread_mutex_lock(&waitLock);
    threadsStarted++;
    ALOGD("thread[%d] wait to run...", (int)arg);
    pthread_cond_wait(&waitCond, &waitLock);
    pthread_mutex_unlock(&waitLock);
    doAtomicTest((int) arg);

    return NULL;
}

static void doBionicTest()
{
#ifdef TEST_BIONIC
    /*
     * Quick function test on the bionic ops.
     */
    int prev;
    int initVal = 7;
    prev = __atomic_inc(&initVal);
    __atomic_inc(&initVal);
    __atomic_inc(&initVal);
    ALOGD("bionic __atomic_inc: %d -> %d\n", prev, initVal);
    prev = __atomic_dec(&initVal);
    __atomic_dec(&initVal);
    __atomic_dec(&initVal);
    ALOGD("bionic __atomic_dec: %d -> %d\n", prev, initVal);
    prev = __atomic_swap(27, &initVal);
    ALOGD("bionic __atomic_swap: %d -> %d\n", prev, initVal);
    int swap_ret = __atomic_cmpxchg(27, 72, &initVal);
    ALOGD("bionic __atomic_cmpxchg: %d (%d)\n", initVal, swap_ret);
#endif
}

int main(void)
{
    ALOGW("-------------AtomicTester begin--------------");

    pthread_t threads[THREAD_COUNT];
    void *(*startRoutine)(void*) = atomicTest;
    int64_t startWhen, endWhen;

#if defined(__ARM_ARCH__)
    ALOGD("__ARM_ARCH__ is %d", __ARM_ARCH__);
#endif
#if defined(ANDROID_SMP)
    ALOGD("ANDROID_SMP is %d", ANDROID_SMP);
#endif
    ALOGD("Creating threads...");

    int i;
    for (i = 0; i < THREAD_COUNT; i++) {
        void* arg = (void*) i;
        if (pthread_create(&threads[i], NULL, startRoutine, arg) != 0) {
            ALOGD("thread create failed");
        }
    }

    /* wait for all the threads to reach the starting line */
    while (1) {
        pthread_mutex_lock(&waitLock);
        if (threadsStarted == THREAD_COUNT) {
            ALOGD("Starting test!");
            startWhen = getRelativeTimeNsec();
            pthread_cond_broadcast(&waitCond);
            pthread_mutex_unlock(&waitLock);
            break;
        }
        pthread_mutex_unlock(&waitLock);
        usleep(100000);
    }

    for (i = 0; i < THREAD_COUNT; i++) {
        void* retval;
        if (pthread_join(threads[i], &retval) != 0) {
            ALOGE("thread join (%d) failed\n", i);
        }
    }

    endWhen = getRelativeTimeNsec();
    if (USE_ATOMIC_TYPE == 3) {
        ALOGD("test pthread_mutex_t with %s", USE_NOINLINE==1 ? "noinline":"always_inline");
    }
    ALOGD("TYPE=%d, All threads stopped, duration time is %.6fms\n",
        USE_ATOMIC_TYPE, (endWhen - startWhen) / 1000000.0);

    /*
     * Show results; expecting:
     *
     * incTest = 5000000
     * decTest = -5000000
     * addTest = 7500000
     * casTest = 10000000
     */
    ALOGD("incTest = %d\n", incTest);
    ALOGD("decTest = %d\n", decTest);
    ALOGD("addTest = %d\n", addTest);
    ALOGD("casTest = %d\n", casTest);

    doBionicTest();

    ALOGW("-------------AtomicTester end--------------");

    return 0;
}
