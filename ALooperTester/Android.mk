LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  ALooperTester.cpp

LOCAL_SHARED_LIBRARIES :=   libbinder   \
                            libutils    \
                            liblog      \
                            libstagefright_foundation

LOCAL_MODULE := ALooperTester

include $(BUILD_EXECUTABLE)

########################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  ReflectorTester.cpp

LOCAL_SHARED_LIBRARIES :=   libbinder   \
                            libutils    \
                            liblog      \
                            libstagefright_foundation

LOCAL_MODULE := ReflectorTester

include $(BUILD_EXECUTABLE)
