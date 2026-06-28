# NW-A105 USB DAC Mode — 顶层编译入口
#
# 用法:
#   make bridge      编译 uac2_bridge daemon (NDK)
#   make clean       清理编译产物
#   make help        显示帮助

NDK ?= /opt/homebrew/share/android-ndk

.PHONY: bridge clean help

bridge:
	@echo "==> 编译 uac2_bridge (arm64, 静态链接) ..."
	$(NDK)/ndk-build \
		NDK_PROJECT_PATH=daemon \
		APP_BUILD_SCRIPT=daemon/Android.mk \
		APP_ABI=arm64-v8a \
		APP_PLATFORM=android-28
	@echo ""
	@echo "==> 产物: daemon/libs/arm64-v8a/uac2_bridge"
	@file daemon/libs/arm64-v8a/uac2_bridge

clean:
	$(NDK)/ndk-build \
		NDK_PROJECT_PATH=daemon \
		APP_BUILD_SCRIPT=daemon/Android.mk \
		APP_ABI=arm64-v8a \
		APP_PLATFORM=android-28 \
		clean
	rm -rf daemon/libs daemon/obj

help:
	@echo "NW-A105 USB DAC 模式 — 编译系统"
	@echo ""
	@echo "用法:"
	@echo "  make bridge      编译 uac2_bridge daemon"
	@echo "  make clean       清理编译产物"
	@echo "  make help        显示此帮助"
	@echo ""
	@echo "环境变量:"
	@echo "  NDK              Android NDK 路径 (默认: /opt/homebrew/share/android-ndk)"
	@echo ""
	@echo "部署:"
	@echo "  adb push daemon/libs/arm64-v8a/uac2_bridge /vendor/bin/"
