LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := AudioRecordDemo.cpp

LOCAL_SHARED_LIBRARIES := libbinder   \
                          libutils    \
                          liblog      \
                          libmedia

LOCAL_MODULE := AudioRecordDemo

include $(BUILD_EXECUTABLE)


###########################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := AudioTrackDemo.cpp

LOCAL_SHARED_LIBRARIES := libbinder   \
                          libutils    \
                          liblog      \
                          libmedia

LOCAL_MODULE := AudioTrackDemo

include $(BUILD_EXECUTABLE)


###########################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := SoundPoolDemo.cpp

LOCAL_SHARED_LIBRARIES := libbinder   \
                          libutils    \
                          liblog      \
                          libmedia

LOCAL_MODULE := SoundPoolDemo

include $(BUILD_EXECUTABLE)
