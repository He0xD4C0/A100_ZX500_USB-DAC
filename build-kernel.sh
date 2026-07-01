#!/bin/bash
#
# build-kernel.sh — 编译 NW-A105 USB DAC 内核
#
# 用法:
#   ./build-kernel.sh                    # 使用 sony-gplsource/ 编译
#   ./build-kernel.sh /path/to/kernel    # 使用自定义内核源码路径
#
# 前题:
#   - 已安装 aarch64-linux-gnu- 交叉编译器 (或 aarch64-none-linux-gnu-)
#   - 已解压 Sony GPL 源码到 sony-gplsource/
#
# 配置来源: 基于 SonyWalkman 稳定内核的 walkman.config (6484 项)
#           仅增加 CONFIG_USB_CONFIGFS_F_UAC2=y (USB DAC 必需)
#
# Copyright 2026 — 基于 GPL v3 发布

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# ─── 内核源码路径 ────────────────────────────────────────
# 优先使用 SonyWalkman 内核树 (含 KernelSU + 其他补丁)
STABLE_KERNEL="/home/he0xd4c0/A100_ZX500_USB-DAC/SonyWalkman/kernel_imx"
if [ $# -ge 1 ]; then
    KERNEL_SRC="$1"
elif [ -d "$STABLE_KERNEL" ]; then
    KERNEL_SRC="$STABLE_KERNEL"
else
    KERNEL_SRC="$PROJECT_ROOT/sony-gplsource/vendor/nxp-opensource/kernel_imx"
fi

# ─── 配置 ────────────────────────────────────────────────
KERNEL_CONFIG="$PROJECT_ROOT/kernel_patches/nwa105_kernel_config"
DTB_FILE_NAME="sony-imx8mm-icx1293"     # NW-A105 硬件代号 (非 dmp1 开发板!)
OUTPUT_IMAGE="$PROJECT_ROOT/Image.gz"

echo "=== NW-A105 USB DAC — 内核编译 ==="
echo "内核源码:   $KERNEL_SRC"
echo "内核配置:   $KERNEL_CONFIG"
echo "DTB 目标:   ${DTB_FILE_NAME}-[0-5].dtb"
echo "输出文件:   $OUTPUT_IMAGE"
echo ""

# ─── 检查 ────────────────────────────────────────────────
if [ ! -d "$KERNEL_SRC" ]; then
    echo "错误: 找不到内核源码: $KERNEL_SRC"
    exit 1
fi
if [ ! -f "$KERNEL_CONFIG" ]; then
    echo "错误: 找不到内核配置: $KERNEL_CONFIG"
    exit 1
fi

# 自动检测交叉编译器 (优先旧版本，4.14 内核对 GCC 11 兼容性最好)
CROSS_COMPILE="aarch64-linux-gnu-"
if command -v aarch64-linux-gnu-gcc-11 &>/dev/null; then
    CC="aarch64-linux-gnu-gcc-11"
    GCC_VER_STR="$(aarch64-linux-gnu-gcc-11 --version | head -1)"
elif command -v aarch64-linux-gnu-gcc-12 &>/dev/null; then
    CC="aarch64-linux-gnu-gcc-12"
    GCC_VER_STR="$(aarch64-linux-gnu-gcc-12 --version | head -1)"
elif command -v aarch64-linux-gnu-gcc &>/dev/null; then
    CC="aarch64-linux-gnu-gcc"
    GCC_VER_STR="$(aarch64-linux-gnu-gcc --version | head -1)"
else
    echo "错误: 未找到 aarch64 交叉编译器"
    echo "请安装: sudo apt install gcc-11-aarch64-linux-gnu"
    exit 1
fi
export CC
echo "→ 交叉编译器: $GCC_VER_STR"

# ─── 应用配置 ────────────────────────────────────────────
echo "→ 应用内核配置 (${KERNEL_CONFIG})..."
cp "$KERNEL_CONFIG" "$KERNEL_SRC/.config"
cd "$KERNEL_SRC"

# ─── GCC 15 兼容性补丁 ───────────────────────────────────
# 1) SELinux classmap.h: 新增 PF_XDP(44) / PF_MCTP(45)
echo "→ 补丁: security/selinux/include/classmap.h..."
if ! grep -q '"xdp_socket"' security/selinux/include/classmap.h; then
    sed -i '/^{ NULL }$/i\\t{ "xdp_socket",\n\t  { COMMON_SOCK_PERMS, NULL } },\n\t{ "mctp_socket",\n\t  { COMMON_SOCK_PERMS, NULL } },' security/selinux/include/classmap.h
fi
sed -i 's/#if PF_MAX > 44/#if PF_MAX > 46/' security/selinux/include/classmap.h

# 2) dtc yylloc 重复定义 (仅当源文件存在时)
if [ -f scripts/dtc/dtc-lexer.lex.c ]; then
    echo "→ 补丁: scripts/dtc/dtc-lexer.lex.c..."
    sed -i 's/^YYLTYPE yylloc;$/extern YYLTYPE yylloc;/' scripts/dtc/dtc-lexer.lex.c
else
    echo "→ dtc-lexer.lex.c 不存在 (将自动生成), 跳过补丁"
fi

# ─── 编译 (对齐 SonyWalkman 稳定构建方式) ──────────────
# KCFLAGS: 选择性抑制警告 (不全局 -w，便于发现真正的问题)
# DTB_FILE_NAME: 使用 icx1293 (NW-A105 硬件), 非 dmp1 (开发板)
echo "→ 开始编译 (${CC}, -j\$(nproc))..."
# 首次切换编译器或配置后需 clean，避免目标文件不一致
make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" clean 2>/dev/null || true
make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" CC="$CC" \
     KCFLAGS="-w -mno-outline-atomics" \
     DTB_FILE_NAME="$DTB_FILE_NAME" \
     -j$(nproc) Image.gz modules dtbs

# ─── 复制产物 ────────────────────────────────────────────
echo "→ 复制 Image.gz..."
cp arch/arm64/boot/Image.gz "$OUTPUT_IMAGE"

echo ""
echo "=== 编译完成 ==="
echo "内核镜像: $OUTPUT_IMAGE"
ls -lh "$OUTPUT_IMAGE"

# 列出生成的 DTB
echo ""
echo "DTB 文件:"
ls -lh arch/arm64/boot/dts/sony/${DTB_FILE_NAME}-*.dtb 2>/dev/null || \
    echo "  (DTB 已内置于内核构建流程)"
