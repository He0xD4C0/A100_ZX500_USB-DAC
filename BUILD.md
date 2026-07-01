# NW-A105 USB DAC — 内核编译与 boot.img 打包指南

## 环境要求

| 组件 | 版本/说明 |
|------|-----------|
| 操作系统 | Ubuntu 20.04+ / Debian 11+（WSL2 可用） |
| 交叉编译器 | `aarch64-linux-gnu-gcc`（GCC 11~15 均支持） |
| 系统包 | `build-essential flex bison bc libssl-dev device-tree-compiler gcc-aarch64-linux-gnu mkbootimg cpio` |
| 源码路径 | `sony-gplsource/vendor/nxp-opensource/kernel_imx/` |
| 原厂固件 | `original_image/A105/boot.img` |
| 工作目录 | 项目根目录（`/home/he0xd4c0/A100_ZX500_USB-DAC/` 或等价路径） |

```bash
sudo apt update
sudo apt install -y build-essential flex bison bc libssl-dev \
    device-tree-compiler gcc-aarch64-linux-gnu mkbootimg cpio
```

## 完整打包流程

### 第一步：进入内核源码目录

```bash
cd sony-gplsource/vendor/nxp-opensource/kernel_imx/
```

### 第二步：确认内核配置

当前 `.config` 已包含所有必需的配置项。验证关键配置：

```bash
# 启动必需（缺失会导致无法开机）
grep -E "CONFIG_DM_VERITY=y|CONFIG_DM_VERITY_FEC=y|CONFIG_DM_BUFIO=y" .config
grep -E "CONFIG_KPROBES=y|CONFIG_KPROBE_EVENTS=y|CONFIG_KRETPROBES=y" .config
grep "CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y" .config
grep "CONFIG_HZ=250" .config

# USB DAC 必需
grep "CONFIG_USB_CONFIGFS_F_UAC2=y" .config
grep "CONFIG_USB_F_UAC2=y" .config
grep "CONFIG_USB_U_AUDIO=y" .config
grep "CONFIG_USB_CHIPIDEA_UDC=y" .config
grep "CONFIG_CONFIGFS_FS=y" .config

# KernelSU
grep "CONFIG_KSU=y" .config
```

如果输出缺少任何项，从 `kernel_patches/nwa105_kernel_config` 重新生成配置并追加缺失项：

```bash
cp ../../kernel_patches/nwa105_kernel_config .config

# 追加启动修复
cat >> .config << 'EOF'
CONFIG_DM_BUFIO=y
CONFIG_DM_VERITY=y
CONFIG_DM_VERITY_FEC=y
CONFIG_DM_VERITY_HASH_PREFETCH_MIN_SIZE=1
CONFIG_KPROBES=y
CONFIG_KPROBE_EVENTS=y
CONFIG_KRETPROBES=y
CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y
EOF

# 启用 USB DAC configfs 控制接口
sed -i 's/# CONFIG_USB_CONFIGFS_F_UAC2 is not set/CONFIG_USB_CONFIGFS_F_UAC2=y/' .config

# 解析依赖
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
```

### 第三步：编译内核

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export DTB_FILE_NAME=sony-imx8mm-icx1293   # ← 必须！控制 DTB 文件名
                                            #    取值: sony-imx8mm-icx1293 (A100 系列)
                                            #          sony-imx8mm-icx1295 (ZX500 系列)

# 首次编译建议先 clean
make clean 2>/dev/null || true

# 编译（使用所有 CPU 核心）
make -j$(nproc)
```

**产物：**

| 文件 | 路径 | 说明 |
|------|------|------|
| 内核镜像 | `arch/arm64/boot/Image` | ARM64 PE/COFF 格式，约 31MB，**未压缩** |
| DTB 变体 0~5 | `arch/arm64/boot/dts/sony/sony-imx8mm-icx1293-{0..5}.dtb` | 6 个硬件变体设备树 |

**GCC 15 兼容性说明：**

内核 Makefile 已内置以下修复，GCC 11~15 均可编译：

```makefile
KBUILD_CFLAGS += $(call cc-option,-Wno-error=incompatible-pointer-types)
KBUILD_CFLAGS += $(call cc-option,-Wno-error=enum-int-mismatch)
KBUILD_CFLAGS += $(call cc-option,-Wno-error=address)
KBUILD_CFLAGS += $(call cc-option,-Wno-error=array-bounds)
```

GPU 驱动 Kbuild 也已修复：`drivers/mxc/gpu-viv/Kbuild` 中 `-Werror` 改为 `-Wno-error`。

### 第四步：提取原厂 ramdisk

`boot.img` = kernel + ramdisk + DTB（占位），其中 ramdisk **必须**从原厂 `boot.img` 提取。

```bash
# 回到项目根目录
cd ../..

python3 << 'EOF'
import struct

with open('original_image/A105/boot.img', 'rb') as f:
    # 读取 Android boot image header（4096 字节）
    header = f.read(4096)

    # 解析关键字段（小端序）
    page_size   = struct.unpack('<I', header[36:40])[0]   # 2048
    kernel_size = struct.unpack('<I', header[8:12])[0]    # 28940800
    ramdisk_size = struct.unpack('<I', header[16:20])[0]  # 12622712

    # 计算 ramdisk 偏移 = page_size + ALIGN(kernel_size, page_size)
    ramdisk_offset = page_size + ((kernel_size + page_size - 1) // page_size) * page_size
    # = 2048 + ((28940800 + 2047) // 2048) * 2048
    # = 2048 + 14133 * 2048
    # = 28944384

    f.seek(ramdisk_offset)
    ramdisk_data = f.read(ramdisk_size)

with open('/tmp/orig_ramdisk.img', 'wb') as out:
    out.write(ramdisk_data)

print(f'Ramdisk: {len(ramdisk_data):,} bytes ({len(ramdisk_data)/1024/1024:.1f} MB compressed)')
EOF

# 验证提取结果
file /tmp/orig_ramdisk.img
# 预期: gzip compressed data
```

### 第五步：打包 boot.img

```bash
mkbootimg \
    --kernel sony-gplsource/vendor/nxp-opensource/kernel_imx/arch/arm64/boot/Image \
    --ramdisk /tmp/orig_ramdisk.img \
    --second sony-gplsource/vendor/nxp-opensource/kernel_imx/arch/arm64/boot/dts/sony/sony-imx8mm-icx1293-2.dtb \
    --cmdline "init=/init androidboot.console=ttymxc1 androidboot.hardware=icx1293 cma=800M@0x400M-0xb7fM androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never androidboot.displaymode=720p buildvariant=user" \
    --base 0x40000000 \
    --kernel_offset 0x00480000 \
    --ramdisk_offset 0x03600000 \
    --second_offset 0x03400000 \
    --tags_offset 0x00400100 \
    --header_version 1 \
    --pagesize 2048 \
    --output output/boot_ksu_dac.img
```

### 第六步：验证产物

```bash
# 检查 boot.img 格式
file output/boot_ksu_dac.img
# 预期输出:
#   Android bootimg, kernel (0x40480000), ramdisk (0x43600000),
#   second stage (0x43400000), page size: 2048, cmdline (...)

# 确认文件大小（约 43MB）
ls -lh output/boot_ksu_dac.img

# 确认关键地址匹配原厂
python3 -c "
import struct
with open('output/boot_ksu_dac.img', 'rb') as f:
    h = f.read(4096)
    ka = struct.unpack('<I', h[12:16])[0]  # kernel_addr
    ra = struct.unpack('<I', h[20:24])[0]  # ramdisk_addr
    sa = struct.unpack('<I', h[28:32])[0]  # second_addr
    ta = struct.unpack('<I', h[32:36])[0]  # tags_addr
    ok = ka==0x40480000 and ra==0x43600000 and sa==0x43400000 and ta==0x40400100
    print(f'kernel_addr=0x{ka:08x} ramdisk_addr=0x{ra:08x} second_addr=0x{sa:08x} tags_addr=0x{ta:08x}')
    print(f'All match original: {ok}')
"
```

## 参数详解

### mkbootimg 参数来源

所有地址参数从原厂 `boot.img` header 提取并验证：

| mkbootimg 参数 | 计算方式 | 最终地址 | 原厂 header 字段 |
|---------------|----------|----------|-----------------|
| `--base` | `tags_addr - 0x100` | `0x40000000` | `tags_addr = 0x40400100` |
| `--kernel_offset` | `kernel_addr - base` | `0x00480000` | `kernel_addr = 0x40480000` |
| `--ramdisk_offset` | `ramdisk_addr - base` | `0x03600000` | `ramdisk_addr = 0x43600000` |
| `--second_offset` | `second_addr - base` | `0x03400000` | `second_addr = 0x43400000` |
| `--tags_offset` | `tags_addr - base` | `0x00400100` | `tags_addr = 0x40400100` |
| `--pagesize` | 直接读取 | `2048` | `page_size = 2048` |
| `--header_version` | 直接读取 | `1` | `header_version = 1` |

### `--second` 参数的重要性

虽然 DTB 实际上由 `dtbo` 分区在运行时提供，**但 `--second` 必须传入一个 DTB 文件**。

原因：U-Boot 的 `do_boota()` 函数将 DTB 从 dtbo 分区复制到 `hdr->second_addr`。如果 `mkbootimg` 未传 `--second`，`second_size=0` 且 `second_addr=0x00000000`，DTB 会被复制到物理地址 0（ARM64 异常向量表），导致内核静默崩溃。

传入 `--second` 强制 `mkbootimg` 计算正确的 `second_addr`：

```
second_addr = base + second_offset = 0x40000000 + 0x03400000 = 0x43400000
```

传入哪个 DTB 变体不重要（实际由 dtbo 分区覆盖），只要让 `second_addr` 非零即可。

### `--kernel` 必须是未压缩 Image

U-Boot 通过 `booti` 命令启动 ARM64 内核。`image_arm64()` 函数检测文件的 PE/COFF 特征：

```
偏移 0x00: 'MZ' (0x4D 0x5A)     — PE/COFF DOS 头
偏移 0x38: 'ARM\x64'            — ARM64 标识
```

**不能用 `Image.gz`、`zImage`、`uImage`。** 只能用 `arch/arm64/boot/Image`。

### 原厂 cmdline

```
init=/init
androidboot.console=ttymxc1
androidboot.hardware=icx1293
cma=800M@0x400M-0xb7fM
androidboot.primary_display=imx-drm
firmware_class.path=/vendor/firmware
transparent_hugepage=never
androidboot.displaymode=720p
buildvariant=user
```

## 一键脚本

以下脚本将上述步骤整合为一条命令：

```bash
#!/bin/bash
# build-and-package.sh — 一键编译内核并打包 boot.img
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
KERNEL_SRC="$PROJECT_ROOT/sony-gplsource/vendor/nxp-opensource/kernel_imx"
OUTPUT="$PROJECT_ROOT/output/boot_ksu_dac.img"
RAMDISK="/tmp/orig_ramdisk.img"

cd "$KERNEL_SRC"
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export DTB_FILE_NAME=sony-imx8mm-icx1293

echo "=== 编译内核 ==="
make -j$(nproc)

echo "=== 提取原厂 ramdisk ==="
python3 -c "
import struct
with open('$PROJECT_ROOT/original_image/A105/boot.img', 'rb') as f:
    h = f.read(4096)
    ps = struct.unpack('<I', h[36:40])[0]
    ks = struct.unpack('<I', h[8:12])[0]
    rs = struct.unpack('<I', h[16:20])[0]
    ro = ps + ((ks + ps - 1) // ps) * ps
    f.seek(ro)
    with open('$RAMDISK', 'wb') as o:
        o.write(f.read(rs))
print(f'Ramdisk: {rs} bytes')
"

echo "=== 打包 boot.img ==="
mkbootimg \
    --kernel "$KERNEL_SRC/arch/arm64/boot/Image" \
    --ramdisk "$RAMDISK" \
    --second "$KERNEL_SRC/arch/arm64/boot/dts/sony/sony-imx8mm-icx1293-2.dtb" \
    --cmdline "init=/init androidboot.console=ttymxc1 androidboot.hardware=icx1293 cma=800M@0x400M-0xb7fM androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never androidboot.displaymode=720p buildvariant=user" \
    --base 0x40000000 \
    --kernel_offset 0x00480000 \
    --ramdisk_offset 0x03600000 \
    --second_offset 0x03400000 \
    --tags_offset 0x00400100 \
    --header_version 1 \
    --pagesize 2048 \
    --output "$OUTPUT"

echo "=== 完成 ==="
file "$OUTPUT"
ls -lh "$OUTPUT"
```

## 部署

```bash
# 推送到 Windows（WSL）
cp output/boot_ksu_dac.img /mnt/c/Users/He0xD/pushfiles/

# 刷入设备
adb reboot bootloader
fastboot flash boot output/boot_ksu_dac.img
fastboot flash dtbo original_image/A105/dtbo.img    # DTB 与原厂字节一致，也可用原厂
fastboot flash vbmeta original_image/A105/blank_vbmeta.img  # 禁用 AVB
fastboot reboot
```

## 故障排查

### 编译错误：`f_uac2.c` 描述符相关

本项目对 `drivers/usb/gadget/function/f_uac2.c` 应用了 Feature Unit 补丁（Windows UAC2 兼容性）。如果文件被修改或还原，特征如下：

- 应有 `USB_OUT_FU_ID=7` / `USB_IN_FU_ID=8` 定义
- 应有 `usb_out_fu_desc` / `io_in_fu_desc` 两个 FU 描述符（`bLength=18`，含 `iFeature`）
- `wTotalLength` 应包含 `sizeof usb_out_fu_desc + sizeof io_in_fu_desc`
- `fs_audio_desc[]` / `hs_audio_desc[]` 应在 `io_in_it_desc` 和 `usb_in_ot_desc` 之间插入两个 FU 条目

### 编译错误：GCC 15 警告变错误

检查顶层 `Makefile` 第 900 行附近是否有 `-Wno-error=` 行。若缺失，添加：

```makefile
KBUILD_CFLAGS += $(call cc-option,-Wno-error=incompatible-pointer-types)
KBUILD_CFLAGS += $(call cc-option,-Wno-error=enum-int-mismatch)
KBUILD_CFLAGS += $(call cc-option,-Wno-error=address)
KBUILD_CFLAGS += $(call cc-option,-Wno-error=array-bounds)
```

### DTB 构建失败：`No rule to make target '-0.dtb'`

原因：`DTB_FILE_NAME` 环境变量未设置。必须 export：

```bash
export DTB_FILE_NAME=sony-imx8mm-icx1293
```

### boot.img 刷入后无法开机

1. 确认 `second_addr` 非零（`file boot.img` 应显示 `second stage (0x43400000)`）
2. 确认 kernel 是未压缩 ARM64 Image（`file arch/arm64/boot/Image` 应显示 `ARM64 boot executable Image`）
3. 确认 `.config` 包含 `CONFIG_DM_VERITY=y` 和 `CONFIG_KPROBES=y`
4. 确认 `vbmeta` 已刷入 `blank_vbmeta.img` 禁用 AVB

### Windows 无法识别 USB 音频设备

1. 确认 f_uac2.c 已应用 Feature Unit 补丁（含 `iFeature` 字段，`bLength=18`）
2. 尝试降低参数：`p_ssize=2` (16-bit), `p_srate=48000`
3. 在设备管理器查看错误代码：`0xA` = Feature Unit 缺失
