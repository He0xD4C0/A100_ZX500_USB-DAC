#!/bin/bash
#
# build-kernel.sh — 编译 NW-A105 USB DAC 内核
#
# 用法:
#   ./build-kernel.sh                    # 使用 sony-gplsource/ 编译
#   ./build-kernel.sh /path/to/kernel    # 使用自定义内核源码路径
#
# 前提:
#   - 已安装 aarch64-linux-gnu- 交叉编译器
#   - 已将索尼 GPL 源码解压到 sony-gplsource/
#     (或通过参数指定路径)
#
# Copyright 2026 — 基于 GPL v3 发布

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# 确定内核源码路径
if [ $# -ge 1 ]; then
    KERNEL_SRC="$1"
else
    KERNEL_SRC="$PROJECT_ROOT/sony-gplsource/vendor/nxp-opensource/kernel_imx"
fi

# 内核配置
KERNEL_CONFIG="$PROJECT_ROOT/kernel_patches/nwa105_kernel_config"

# 输出
OUTPUT_IMAGE="$PROJECT_ROOT/Image.gz"

echo "=== NW-A105 USB DAC — 内核编译 ==="
echo "内核源码: $KERNEL_SRC"
echo "内核配置: $KERNEL_CONFIG"
echo "输出文件: $OUTPUT_IMAGE"
echo ""

# 检查内核源码是否存在
if [ ! -d "$KERNEL_SRC" ]; then
    echo "错误: 找不到内核源码目录: $KERNEL_SRC"
    echo ""
    echo "请先将索尼 GPL 源码包解压到 sony-gplsource/:"
    echo "  tar -xzf gpl_source.tgz -C sony-gplsource/"
    echo ""
    echo "或者传递自定义路径:"
    echo "  $0 /path/to/kernel_imx"
    exit 1
fi

# 检查内核配置文件
if [ ! -f "$KERNEL_CONFIG" ]; then
    echo "错误: 找不到内核配置文件: $KERNEL_CONFIG"
    exit 1
fi

# 检查交叉编译器
if ! command -v aarch64-linux-gnu-gcc &>/dev/null; then
    echo "错误: 找不到 aarch64-linux-gnu-gcc"
    echo "请安装交叉编译器:"
    echo "  sudo apt install gcc-aarch64-linux-gnu"
    exit 1
fi

# 应用内核配置
echo "→ 应用内核配置..."
cp "$KERNEL_CONFIG" "$KERNEL_SRC/.config"

# 编译内核
echo "→ 开始编译 (CROSS_COMPILE=aarch64-linux-gnu-)..."
cd "$KERNEL_SRC"
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
     KCFLAGS='-w' -j$(nproc) Image.gz modules

# 复制编译产物
echo "→ 复制 Image.gz..."
cp arch/arm64/boot/Image.gz "$OUTPUT_IMAGE"

echo ""
echo "=== 编译完成 ==="
echo "内核镜像: $OUTPUT_IMAGE"
ls -lh "$OUTPUT_IMAGE"
