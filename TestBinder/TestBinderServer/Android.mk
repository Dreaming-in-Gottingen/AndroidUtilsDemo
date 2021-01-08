LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ITestBinderService.cpp \
                   TestBinderService.cpp

LOCAL_SHARED_LIBRARIES := libbinder libutils liblog

LOCAL_MODULE := libTestBinderService

include $(BUILD_SHARED_LIBRARY)

#######################################################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  main_testBinder.cpp \

LOCAL_SHARED_LIBRARIES := libbinder   \
                          libutils    \
                          liblog      \
                          libTestBinderService      \

LOCAL_MODULE := testBinderServer

include $(BUILD_EXECUTABLE)
