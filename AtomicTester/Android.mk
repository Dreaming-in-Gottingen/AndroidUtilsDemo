LOCAL_PATH:= $(call my-dir)

########################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := AtomicTester.cpp

LOCAL_SHARED_LIBRARIES := libcutils   \
                          libutils    \
                          liblog      \

LOCAL_MODULE := AtomicTester
include $(BUILD_EXECUTABLE)
