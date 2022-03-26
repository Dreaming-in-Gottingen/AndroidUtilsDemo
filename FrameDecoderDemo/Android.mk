LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= FrameDecoderDemo.cpp

LOCAL_SHARED_LIBRARIES := liblog \
                          libutils \
                          libcutils \
                          libbinder \
                          libstagefright \
                          libstagefright_foundation \

LOCAL_C_INCLUDES := \
    frameworks/av/media/libstagefright

LOCAL_CFLAGS := -Wno-unused-variable

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= framedecoder_demo

include $(BUILD_EXECUTABLE)
