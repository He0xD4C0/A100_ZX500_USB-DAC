#!/bin/bash
#
# build-bootimg.sh — 打包 flashable boot.img (NW-A105 / DMP1)
#
# 用法:
#   ./build-bootimg.sh                     # 自动使用 original_image/boot.img 的 ramdisk
#   ./build-bootimg.sh /path/to/boot.img   # 从指定 boot.img 提取 ramdisk
#
# 输出: boot.img (可 fastboot flash boot 刷入)
#
# Copyright 2026 — 基于 GPL v3 发布

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# 路径
KERNEL_IMAGE="$PROJECT_ROOT/sony-gplsource/vendor/nxp-opensource/kernel_imx/arch/arm64/boot/Image"
KERNEL_DTB="$PROJECT_ROOT/sony-gplsource/vendor/nxp-opensource/kernel_imx/arch/arm64/boot/dts/sony/sony-imx8mm-dmp1.dtb"
OUTPUT_IMG="$PROJECT_ROOT/boot.img"

# 原厂 boot.img —— 优先用参数指定的，其次用 original_image/ 下的
DEFAULT_STOCK="$PROJECT_ROOT/original_image/A105/boot.img"
if [ $# -ge 1 ]; then
    STOCK_BOOTIMG="$1"
elif [ -f "$DEFAULT_STOCK" ]; then
    STOCK_BOOTIMG="$DEFAULT_STOCK"
else
    STOCK_BOOTIMG=""
fi

TMPDIR=""

cleanup() {
    [ -n "$TMPDIR" ] && [ -d "$TMPDIR" ] && rm -rf "$TMPDIR"
}
trap cleanup EXIT

TMPDIR="$(mktemp -d)"

echo "=== NW-A105 USB DAC — boot.img 打包 ==="
echo ""

# ------------------------------------------------------------------
# 检查输入
# ------------------------------------------------------------------
if [ ! -f "$KERNEL_IMAGE" ]; then
    echo "错误: 找不到内核 Image: $KERNEL_IMAGE"
    echo "请先运行 ./build-kernel.sh 编译内核"
    exit 1
fi

if [ ! -f "$KERNEL_DTB" ]; then
    echo "错误: 找不到 DTB: $KERNEL_DTB"
    exit 1
fi

if ! command -v mkbootimg &>/dev/null; then
    echo "错误: 未安装 mkbootimg"
    echo "安装: sudo apt install mkbootimg"
    exit 1
fi

# ------------------------------------------------------------------
# 提取原厂 boot.img 参数和 ramdisk
# ------------------------------------------------------------------
RAMDISK_FILE="$TMPDIR/ramdisk.img"

# NW-A105 (DMP1 / icx1293) 原厂默认值，会被 stock boot.img 覆盖
BASE="0x40400000"
KERNEL_OFFSET="0x00080000"
RAMDISK_OFFSET="0x03200000"
TAGS_OFFSET="0x00000100"
PAGESIZE="2048"
HEADER_VERSION="1"
OS_VERSION="9.0.0"
OS_PATCH_LEVEL="2021-11"
CMDLINE="init=/init androidboot.console=ttymxc1 androidboot.hardware=icx1293 cma=800M@0x400M-0xb7fM androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never androidboot.displaymode=720p buildvariant=user"

if [ -n "$STOCK_BOOTIMG" ] && [ -f "$STOCK_BOOTIMG" ]; then
    echo "→ 提取原厂 boot.img: $STOCK_BOOTIMG"
    if ! command -v unpack_bootimg &>/dev/null; then
        echo "错误: 需要 unpack_bootimg"
        echo "安装: sudo apt install mkbootimg"
        exit 1
    fi

    EXTRACT_DIR="$TMPDIR/extract"
    mkdir -p "$EXTRACT_DIR"

    # 捕获 unpack_bootimg 输出以解析参数
    UNPACK_OUTPUT=$(unpack_bootimg --boot_img "$STOCK_BOOTIMG" --out "$EXTRACT_DIR" 2>&1)
    echo "$UNPACK_OUTPUT"

    # 从输出中解析参数
    parse_val() { echo "$UNPACK_OUTPUT" | grep "$1:" | head -1 | sed "s/.*${1}: *//"; }

    KERNEL_LOAD_ADDR=$(parse_val "kernel load address")
    RAMDISK_LOAD_ADDR=$(parse_val "ramdisk load address")
    TAGS_LOAD_ADDR=$(parse_val "kernel tags load address")
    PAGESIZE=$(parse_val "page size")
    HEADER_VERSION=$(parse_val "boot image header version")
    OS_VERSION=$(parse_val "os version")
    OS_PATCH_LEVEL=$(parse_val "os patch level")
    STOCK_CMDLINE=$(parse_val "command line args")

    # 计算 offsets
    if [ -n "$TAGS_LOAD_ADDR" ]; then
        BASE="$TAGS_LOAD_ADDR"
        # base = tags_addr - 0x100 (standard Android convention)
        BASE=$(printf "0x%x" $(( TAGS_LOAD_ADDR - 0x100 )))
    fi
    if [ -n "$KERNEL_LOAD_ADDR" ] && [ -n "$BASE" ]; then
        KERNEL_OFFSET=$(printf "0x%x" $(( KERNEL_LOAD_ADDR - BASE )))
    fi
    if [ -n "$RAMDISK_LOAD_ADDR" ] && [ -n "$BASE" ]; then
        RAMDISK_OFFSET=$(printf "0x%x" $(( RAMDISK_LOAD_ADDR - BASE )))
    fi
    TAGS_OFFSET="0x00000100"

    # 使用 stock cmdline
    if [ -n "$STOCK_CMDLINE" ]; then
        CMDLINE="$STOCK_CMDLINE"
    fi

    # 复制 ramdisk
    cp "$EXTRACT_DIR"/ramdisk "$RAMDISK_FILE"
    echo "  → ramdisk: $(ls -lh "$RAMDISK_FILE" | awk '{print $5}')"
    echo "  → cmdline: $CMDLINE"
else
    # 无原厂 boot.img —— 创建最小 ramdisk
    echo "→ 无原厂 boot.img，创建最小 ramdisk..."
    echo "# Dummy ramdisk for USB-DAC kernel" > "$TMPDIR/dummy.txt"
    (cd "$TMPDIR" && echo "dummy.txt" | cpio -o -H newc 2>/dev/null) | gzip > "$RAMDISK_FILE"
    echo "  → 最小 ramdisk: $(ls -lh "$RAMDISK_FILE" | awk '{print $5}')"
fi

# ------------------------------------------------------------------
# 组装 boot.img
# ------------------------------------------------------------------
echo ""
echo "→ 组装 boot.img..."
echo "  base:           $BASE"
echo "  kernel_offset:  $KERNEL_OFFSET"
echo "  ramdisk_offset: $RAMDISK_OFFSET"
echo "  tags_offset:    $TAGS_OFFSET"
echo "  pagesize:       $PAGESIZE"
echo "  header_version: $HEADER_VERSION"
echo "  os_version:     $OS_VERSION"
echo "  os_patch_level: $OS_PATCH_LEVEL"
echo "  cmdline:        $CMDLINE"

mkbootimg \
    --kernel "$KERNEL_IMAGE" \
    --ramdisk "$RAMDISK_FILE" \
    --dtb "$KERNEL_DTB" \
    --base "$BASE" \
    --kernel_offset "$KERNEL_OFFSET" \
    --ramdisk_offset "$RAMDISK_OFFSET" \
    --tags_offset "$TAGS_OFFSET" \
    --pagesize "$PAGESIZE" \
    --header_version "$HEADER_VERSION" \
    --os_version "$OS_VERSION" \
    --os_patch_level "$OS_PATCH_LEVEL" \
    --cmdline "$CMDLINE" \
    --output "$OUTPUT_IMG"

echo ""
echo "=== 打包完成 ==="
echo "boot.img: $OUTPUT_IMG"
ls -lh "$OUTPUT_IMG"

echo ""
echo "刷入方法:"
echo "  adb reboot bootloader"
echo "  fastboot flash boot boot.img"
echo "  fastboot reboot"
