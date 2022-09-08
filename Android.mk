LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := jnigraphics-prebuilt
LOCAL_SRC_FILES := /home/mongoose/rawdrawandroid/makecapk/lib/arm64-v8a/libjnigraphics.so
include $(PREBUILT_SHARED_LIBRARY)