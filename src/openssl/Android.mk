LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := openssl_crypto
LOCAL_SRC_FILES := libs/arm64-v8a/libcrypto.so
LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := openssl_ssl
LOCAL_SRC_FILES := libs/arm64-v8a/libssl.so
LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)
