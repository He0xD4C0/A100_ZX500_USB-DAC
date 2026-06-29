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
cd "$KERNEL_SRC"

# ============================================================
# 工具链兼容性修复 (GCC 15 / glibc 2.40+)
# ============================================================

# 1) SELinux classmap.h 补丁 —— 新 glibc 新增了 PF_XDP(44) 和
#    PF_MCTP(45) 两个地址族，4.14 内核的 secclass_map 缺少对应条目。
#    追加 xdp_socket / mctp_socket 条目，更新 PF_MAX 检查值 44→46。
#    完全不关闭 SELinux，对内核运行时无任何影响。
echo "→ 修复: security/selinux/include/classmap.h (新增 PF_XDP/PF_MCTP)..."
if ! grep -q '"xdp_socket"' security/selinux/include/classmap.h; then
    sed -i '/^{ NULL }$/i\\t{ "xdp_socket",\n\t  { COMMON_SOCK_PERMS, NULL } },\n\t{ "mctp_socket",\n\t  { COMMON_SOCK_PERMS, NULL } },' security/selinux/include/classmap.h
fi
sed -i 's/#if PF_MAX > 44/#if PF_MAX > 46/' security/selinux/include/classmap.h

# 2) 修复 dtc yylloc 重复定义 —— 新版 flex/bison 导致
#    dtc-parser.tab.c 和 dtc-lexer.lex.c 中都定义了 yylloc。
#    将 lexer 中的定义改为 extern 声明，保留 parser 中的唯一定义。
echo "→ 修复: dtc yylloc 重复定义..."
sed -i 's/^YYLTYPE yylloc;$/extern YYLTYPE yylloc;/' scripts/dtc/dtc-lexer.lex.c

# 准备内核构建环境（生成 include/config/auto.conf 等）
echo "→ 准备构建环境 (olddefconfig)..."
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig

# 清理上次构建的中间产物，避免 .tmp_*.o / .tmp_*.ver 残留导致链接失败
echo "→ 清理中间产物 (make clean)..."
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean

# 编译内核（-j4 比 -j$(nproc) 更稳定，4.14 内核在高并发下容易出现竞态）
echo "→ 开始编译 (CROSS_COMPILE=aarch64-linux-gnu-)..."
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
     KCFLAGS='-w' -j4 Image.gz modules

# 复制编译产物
echo "→ 复制 Image.gz..."
cp arch/arm64/boot/Image.gz "$OUTPUT_IMAGE"

echo ""
echo "=== 编译完成 ==="
echo "内核镜像: $OUTPUT_IMAGE"
ls -lh "$OUTPUT_IMAGE"
