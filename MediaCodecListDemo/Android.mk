LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := MediaCodecListDemo.cpp \

LOCAL_C_INCLUDES := $(TOP)/frameworks/av/include

LOCAL_SHARED_LIBRARIES := libutils       \
                          liblog         \
                          libstagefright \
                          libstagefright_foundation  \

LOCAL_MODULE := MediaCodecListDemo

include $(BUILD_EXECUTABLE)
