LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := picoc

LOCAL_SRC_FILES := \
    clibrary.c \
    debug.c \
    expression.c \
    heap.c \
    include.c \
    lex.c \
    parse.c \
    picoc_ezApp.c \
    platform.c \
    table.c \
    type.c \
    variable.c \
    cstdlib/ctype.c \
    cstdlib/errno.c \
    cstdlib/math.c \
    cstdlib/stdbool.c \
    cstdlib/stdio.c \
    cstdlib/stdlib.c \
    cstdlib/string.c \
    cstdlib/time.c \
    cstdlib/unistd.c \
    cstdlib/libgen.c \
    cstdlib/fcntl.c \
    platform/library_unix.c \
    platform/platform_unix.c

LOCAL_CFLAGS := -DUNIX_HOST -O2

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/../ezApp_lib/include

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include

include $(BUILD_STATIC_LIBRARY)
