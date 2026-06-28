#
# Copyright (C) 2014 The Android Open Source Project
# Copyright 2015 Sony Corporation.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

LOCAL_PATH := $(call my-dir)

#
# i2c-tools pseudo module
#
include $(CLEAR_VARS)
LOCAL_MODULE := i2c-tools
LOCAL_MODULE_TAGS := debug
LOCAL_REQUIRED_MODULES := i2cdetect i2cdump i2cset i2cget
include $(BUILD_PHONY_PACKAGE)


#
# Build i2cdetect command
#
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
		   -Wcast-align -Wwrite-strings -Wnested-externs -Winline \
		   -W -Wundef -Wmissing-prototypes -Iinclude
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

#LOCAL_SRC_FILES := i2cbusses.c i2cdetect.c i2cdump.c i2cset.c i2cget.c util.c
LOCAL_SRC_FILES := tools/i2cdetect.c tools/i2cbusses.c lib/smbus.c
LOCAL_MODULE := i2cdetect
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
#LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)

#
# Build i2cdump command
#
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
		   -Wcast-align -Wwrite-strings -Wnested-externs -Winline \
		   -W -Wundef -Wmissing-prototypes -Iinclude
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

#LOCAL_SRC_FILES := i2cbusses.c i2cdetect.c i2cdump.c i2cset.c i2cget.c util.c
LOCAL_SRC_FILES := tools/i2cdump.c tools/i2cbusses.c lib/smbus.c tools/util.c
LOCAL_MODULE := i2cdump
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
#LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)

#
# Build i2cset command
#
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
		   -Wcast-align -Wwrite-strings -Wnested-externs -Winline \
		   -W -Wundef -Wmissing-prototypes -Iinclude
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

#LOCAL_SRC_FILES := i2cbusses.c i2cdetect.c i2cdump.c i2cset.c i2cget.c util.c
LOCAL_SRC_FILES := tools/i2cset.c tools/i2cbusses.c lib/smbus.c tools/util.c
LOCAL_MODULE := i2cset
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
#LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)

#
# Build i2cget command
#
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
		   -Wcast-align -Wwrite-strings -Wnested-externs -Winline \
		   -W -Wundef -Wmissing-prototypes -Iinclude
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

#LOCAL_SRC_FILES := i2cbusses.c i2cdetect.c i2cdump.c i2cset.c i2cget.c util.c
LOCAL_SRC_FILES := tools/i2cget.c tools/i2cbusses.c lib/smbus.c tools/util.c
LOCAL_MODULE := i2cget
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
#LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)
