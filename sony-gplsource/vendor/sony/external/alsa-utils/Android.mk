
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
ifeq ($(strip $(BUILD_WITH_ALSA_UTILS)),true)

LOCAL_PATH:= $(call my-dir)

#
# Build aplay command
#

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
	-fPIC -D_POSIX_SOURCE \
	-DALSA_CONFIG_DIR=\"/vendor/usr/share/alsa\" \
	-DALSA_PLUGIN_DIR=\"/vendor/usr/lib/alsa-lib\" \
	-DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
	-DALSA_LINUX_TARGET

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/android \
	vendor/sony/external/alsa-lib

LOCAL_SRC_FILES := \
	aplay/aplay.c

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := alsa_aplay
LOCAL_VENDOR_MODULE := true


LOCAL_SHARED_LIBRARIES := libasound libc

LOCAL_NOTICE_FILE := $(LOCAL_PATH)/COPYING

include $(BUILD_EXECUTABLE)

#
# Build hw_param command
#

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
	-fPIC -D_POSIX_SOURCE \
	-DALSA_CONFIG_DIR=\"/vendor/usr/share/alsa\" \
	-DALSA_PLUGIN_DIR=\"/vendor/usr/lib/alsa-lib\" \
	-DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
	-DALSA_LINUX_TARGET

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/android \
	vendor/sony/external/alsa-lib

LOCAL_SRC_FILES := \
	hw_params/hw_params.c

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := alsa_hw_params
LOCAL_VENDOR_MODULE := true

LOCAL_SHARED_LIBRARIES := libasound libc

LOCAL_NOTICE_FILE := $(LOCAL_PATH)/COPYING

include $(BUILD_EXECUTABLE)

#
# Build alsaloop command
#

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
        -fPIC -D_POSIX_SOURCE \
        -DALSA_CONFIG_DIR=\"/vendor/usr/share/alsa\" \
        -DALSA_PLUGIN_DIR=\"/vendor/usr/lib/alsa-lib\" \
        -DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
        -DALSA_LINUX_TARGET

LOCAL_C_INCLUDES:= \
        $(LOCAL_PATH)/include \
        $(LOCAL_PATH)/android \
        vendor/sony/external/alsa-lib

LOCAL_SRC_FILES := \
        alsaloop/alsaloop.c \
        alsaloop/control.c \
        alsaloop/pcmjob.c 

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := alsa_loop
LOCAL_VENDOR_MODULE := true


LOCAL_SHARED_LIBRARIES := libasound libc

LOCAL_NOTICE_FILE := $(LOCAL_PATH)/COPYING

include $(BUILD_EXECUTABLE)

#
# Build alsactl command
#

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
	-fPIC -D_POSIX_SOURCE \
	-DALSA_CONFIG_DIR=\"/vendor/usr/share/alsa\" \
	-DALSA_PLUGIN_DIR=\"/vendor/usr/lib/alsa-lib\" \
	-DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
	-DALSA_LINUX_TARGET

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/android \
	vendor/sony/external/alsa-lib

LOCAL_SRC_FILES := \
	alsactl/alsactl.c \
	alsactl/init_parse.c \
	alsactl/state.c \
	alsactl/utils.c

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := alsa_ctl
LOCAL_VENDOR_MODULE := true

LOCAL_SHARED_LIBRARIES := libasound libc

LOCAL_NOTICE_FILE := $(LOCAL_PATH)/COPYING

include $(BUILD_EXECUTABLE)

#
# Build amixer command
#

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
	-fPIC -D_POSIX_SOURCE \
	-DALSA_CONFIG_DIR=\"/vendor/usr/share/alsa\" \
	-DALSA_PLUGIN_DIR=\"/vendor/usr/lib/alsa-lib\" \
	-DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
	-DALSA_LINUX_TARGET

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/android \
	vendor/sony/external/alsa-lib

LOCAL_SRC_FILES := \
	amixer/amixer.c

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := alsa_amixer
LOCAL_VENDOR_MODULE := true

LOCAL_SHARED_LIBRARIES := libasound libc

LOCAL_NOTICE_FILE := $(LOCAL_PATH)/COPYING

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

ALSAINIT_DIR := $(TARGET_OUT)/usr/share/alsa/init

files := $(addprefix $(ALSAINIT_DIR)/,00main default hda help info test)

$(files): PRIVATE_MODULE := alsactl_initdir
$(files): $(ALSAINIT_DIR)/%: $(LOCAL_PATH)/alsactl/init/% | $(ACP)
	$(transform-prebuilt-to-target)

#ALL_PREBUILT += $(files)

endif
endif
