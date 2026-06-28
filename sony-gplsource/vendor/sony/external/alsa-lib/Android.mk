# external/alsa-lib/Android.mk
#
# Copyright 2008 Wind River Systems
#

ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE       := libasound
LOCAL_MODULE_TAGS  := debug
LOCAL_PRELINK_MODULE := false
LOCAL_VENDOR_MODULE := true
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

# libasound must be compiled with -fno-short-enums, as it makes extensive
# use of enums which are often type casted to unsigned ints.
LOCAL_CFLAGS := \
	-O -fPIC -DPIC -D_POSIX_SOURCE \
	-DALSA_CONFIG_DIR=\"/vendor/usr/share/alsa\" \
	-DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
	-include asm/types.h \
	-DANDROID \
	-DALSA_LINUX_TARGET \
	-Wno-error=implicit-function-declaration


LOCAL_SRC_FILES := $(sort $(call all-c-files-under, src))

# It is easier to exclude the ones we don't want...
#
LOCAL_SRC_FILES := $(filter-out src/alisp/alisp.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/alisp/alisp_snd.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/compat/hsearch_r.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/control/control_shm.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/pcm/pcm_d%.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/pcm/pcm_ladspa.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/pcm/pcm_shm.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/pcm/scopes/level.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out src/shmarea.c, $(LOCAL_SRC_FILES))

ifeq ($(TARGET_ARCH),arm)
  LOCAL_SDK_VERSION := 9
endif


# LOCAL_COPY_HEADERS_TO := $(common_COPY_HEADERS_TO)
# LOCAL_COPY_HEADERS := $(common_COPY_HEADERS)
LOCAL_SHARED_LIBRARIES := \
    libdl\

LOCAL_NOTICE_FILE := $(LOCAL_PATH)/COPYING

include $(BUILD_SHARED_LIBRARY)
endif
