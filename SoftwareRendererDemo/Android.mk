LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= SoftwareRendererDemo.cpp

LOCAL_SHARED_LIBRARIES := liblog \
                          libutils \
                          libcutils \
                          libui \
                          libgui \
                          libstagefright \
                          libstagefright_foundation \
                          libstagefright_codecbase \

LOCAL_STATIC_LIBRARIES := \
	libstagefright_color_conversion \
	libyuv_static \

LOCAL_C_INCLUDES := \
	frameworks/av/media/libstagefright

LOCAL_CFLAGS := -Wno-unused-variable

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= softrenderer_demo

include $(BUILD_EXECUTABLE)
