LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := RefBaseTester.cpp

LOCAL_SHARED_LIBRARIES := libbinder   \
                          libutils    \
                          liblog      \

LOCAL_MODULE := RefBaseTester

include $(BUILD_EXECUTABLE)
