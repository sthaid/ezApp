LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := lodepng

LOCAL_SRC_FILES := lodepng.c

LOCAL_CFLAGS := -O2

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include

include $(BUILD_STATIC_LIBRARY)
