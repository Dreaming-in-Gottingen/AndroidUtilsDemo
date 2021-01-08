LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := testBinder.cpp \

LOCAL_SHARED_LIBRARIES := libbinder   \
                          libutils    \
                          liblog      \
                          libTestBinderService      \

LOCAL_MODULE := testBinder

include $(BUILD_EXECUTABLE)
