LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= SurfaceDemo.cpp

LOCAL_SHARED_LIBRARIES := liblog \
                          libutils \
                          libcutils \
                          libui \
                          libgui \
                          libstagefright \
                          libstagefright_foundation \

LOCAL_CFLAGS := -Wno-unused-variable

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= surface_demo

include $(BUILD_EXECUTABLE)
