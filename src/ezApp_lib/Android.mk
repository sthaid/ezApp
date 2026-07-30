LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ezApp_lib

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/../SDL/include \
                    $(LOCAL_PATH)/../SDL_mixer/include \
                    $(LOCAL_PATH)/../SDL_ttf/include \
                    $(LOCAL_PATH)/../libmp3lame/include \
                    $(LOCAL_PATH)/../cJSON/include \
                    $(LOCAL_PATH)/../kissfft/include \
                    $(LOCAL_PATH)/../lodepng/include \
                    $(LOCAL_PATH)/../picoc/include \
                    $(LOCAL_PATH)/../openssl/include

LOCAL_SRC_FILES := \
    logging.c \
    run.c \
    sdlx_audio.c \
    sdlx_event.c \
    sdlx_misc.c \
    sdlx_sensor.c \
    sdlx_video.c \
    svcs.c \
    utils.c \
    utils_openssl.c \
    utils_android.cpp 

LOCAL_CFLAGS := -O2

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include

include $(BUILD_STATIC_LIBRARY)
