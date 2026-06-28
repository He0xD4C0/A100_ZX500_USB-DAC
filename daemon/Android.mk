#
# NW-A105 USB DAC — Android NDK 编译脚本
#
# 用法:
#   export NDK=/path/to/android-ndk-r21e
#   $NDK/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk \
#                   APP_ABI=arm64-v8a APP_PLATFORM=android-28
#
# 产物:
#   dacd         — 原生守护进程（推荐，取代 shell 脚本）
#   uac2_bridge  — 独立桥接工具（兼容保留）
#

LOCAL_PATH := $(call my-dir)

# ─── dacd: 原生守护进程 ──────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE    := dacd
LOCAL_SRC_FILES := dacd.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := libtinyalsa
LOCAL_LDFLAGS   := -static
LOCAL_CFLAGS    := -O2 -Wall -Wextra -std=gnu99 -DSSIZE_MAX=__LONG_MAX__
include $(BUILD_EXECUTABLE)

# ─── uac2_bridge: 独立桥接工具（兼容保留） ───────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE    := uac2_bridge
LOCAL_SRC_FILES := uac2_bridge.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := libtinyalsa
LOCAL_LDFLAGS   := -static
LOCAL_CFLAGS    := -O2 -Wall -Wextra -DSSIZE_MAX=__LONG_MAX__
include $(BUILD_EXECUTABLE)

#
# 依赖: libtinyalsa 静态库
# 可以从 A105 的 /system/lib64/libtinyalsa.so 提取头文件,
# 或从 sony-gplsource/external/tinyalsa/ 获取 AOSP 源码自行编译
#
include $(CLEAR_VARS)
LOCAL_MODULE    := libtinyalsa
LOCAL_SRC_FILES := tinyalsa/src/pcm.c tinyalsa/src/mixer.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/tinyalsa/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include/tinyalsa
include $(BUILD_STATIC_LIBRARY)
