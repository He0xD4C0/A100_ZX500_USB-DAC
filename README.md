# NW-A105 USB DAC 模式

将 Sony NW-A105 Walkman 变成 USB DAC——PC/Mac 通过 USB 将音频发送至 A105，由机内的 **CXD3778GF** 芯片解码并从耳机孔输出。

```
PC / Mac
   │  USB (UAC2)
   ▼
NW-A105 (USB Gadget, ci_hdrc.0)
   │  f_uac2 → ALSA card#2 (capture)
   │  uac2_bridge daemon
   ▼
imx-audio-cxd3778gf (card#1, playback)
   │  SAI → CXD3778GF DAC
   ▼
耳机孔输出
```

---

## 硬件要求

| 项目 | 说明 |
|---|---|
| 设备 | Sony NW-A105 (ICX1293，Android 9，内核 4.14.78) |
| Root | KernelSU 或 Magisk（必须） |
| PC 端 | 任意支持 USB Audio Class 2 的系统（Windows/macOS/Linux） |
| 数据线 | 支持数据传输的 USB-C 线 |

---

## 项目结构

```
A105_DAC_Mode/
├── Image.gz                 # 已编译的内核镜像（含 UAC2 驱动）
├── boot.img                 # 打包好的 Android boot image（可 fastboot 刷入）
├── build-kernel.sh          # 内核一键编译脚本
├── build-bootimg.sh         # boot.img 打包脚本（自动使用原厂 ramdisk）
├── PLAN.md                  # 详细技术方案
├── sony-gplsource/          # 索尼 GPL 源码（完整 AOSP 内核树）
├── original_image/          # 原厂固件镜像（ramdisk 提取源）
│   └── A105/
│       ├── boot.img         #   原厂 boot.img（ramdisk + 参数参考）
│       ├── bootloader.img
│       ├── dtbo.img
│       ├── system.img
│       ├── vendor.img
│       └── vbmeta.img
├── activity/                # Android 控制 App（Jetpack Compose）
│   └── app/src/main/
│       ├── assets/uac2_bridge   # 已编译的桥接守护进程（arm64 静态）
│       └── java/.../MainActivity.kt
├── daemon/
│   ├── uac2_bridge.c        # 桥接守护进程源码
│   └── tinyalsa/            # 依赖（git submodule）
└── kernel_patches/          # 内核修改记录
```

---

## 部署步骤

### 第一步：打包 boot.img

内核编译完成后，需要将内核、DTB、原厂 ramdisk 打包为 Android boot image：

```bash
# 自动使用 original_image/A105/boot.img 中的原厂 ramdisk 和参数
./build-bootimg.sh
```

脚本会从原厂 `boot.img` 中提取完整 ramdisk（含 init / sepolicy / fstab 等），
并用以下（从 boot_b 实测的）参数组装新 boot.img：

| 参数 | 值 |
|------|-----|
| Base | `0x40400000` |
| Kernel addr | `0x40480000` |
| Ramdisk addr | `0x43600000` |
| Tags addr | `0x40400100` |
| Page size | 2048 |
| Header version | 1 (v1) |
| Kernel 格式 | 未压缩 ARM64 PE/COFF Image (`MZ` 头) |

关键 cmdline：`init=/init androidboot.hardware=icx1293 cma=800M@0x400M-0xb7fM ...`

### 第二步：刷入内核

```bash
# 重启到 Bootloader 模式
adb reboot bootloader

# 刷入 boot.img（保留 system/vendor 分区不变）
fastboot flash boot boot.img

# 重启
fastboot reboot
```

验证内核是否包含 UAC2 支持：

```bash
adb shell "zcat /proc/config.gz | grep UAC2"
# 预期输出: CONFIG_USB_CONFIGFS_F_UAC2=y
```

### 第三步：安装控制 App

用 Android Studio 打开 `activity/` 目录，然后直接 **Run → Run 'app'** 安装到设备。

App 内置了已编译好的 `uac2_bridge` 守护进程，**无需手动推送任何文件**。

### 第四步：授予 Root 权限

首次打开 App 时，Magisk / KernelSU 会弹出授权弹窗，选择"授权"即可。

### 第五步：使用

1. 将 NW-A105 通过 USB-C 数据线连接到 PC
2. 打开 **USB DAC** App
3. 打开 **USB DAC 主开关**
4. PC 端将识别出一个新的 USB 音频设备，将系统默认播放设备切换到它即可

---

## App 功能说明

| 选项 | 说明 |
|---|---|
| **USB DAC 主开关** | 开启/关闭 DAC 模式 |
| **保留 ADB** | DAC 开启时是否同时维持 ADB 连接。关闭此项可减少 USB 枚举冲突，ADB 在 DAC 关闭后自动恢复 |
| **低功耗 PCM 输出** | 选用 `pcmC1D2p`（低功耗 Hi-Res）而非 `pcmC1D0p`（标准 Hi-Res），在低发热场景下有用 |

**音频格式自适应**：App 将 UAC2 端口声明为最大 192 kHz / 24-bit / 立体声。实际格式由 PC 端与设备自动协商（USB Audio 标准行为），`uac2_bridge` 启动后会读取 `/proc/asound` 自动适配，无需用户手动配置。

---

## 从源码重新编译

### 编译 Bridge 守护进程

```bash
# 在 WSL Ubuntu 中执行
cd daemon/
git submodule update --init --recursive   # 拉取 tinyalsa

aarch64-linux-gnu-gcc -static -O2 \
    -I./tinyalsa/include \
    -o uac2_bridge uac2_bridge.c \
    ./tinyalsa/src/pcm.c \
    ./tinyalsa/src/mixer.c \
    -lpthread

# 将编译好的二进制更新到 App assets
cp uac2_bridge ../activity/app/src/main/assets/uac2_bridge
```

### 获取内核源码

本项目**已包含**索尼在 GPL 协议下发布的 **NW-A100 / NW-ZX500 系列 Linux 源码**（gpl_source.tgz，版本 20211130），解压在 `sony-gplsource/` 目录下。

内核基于索尼 GPL 源码包中的 `vendor/nxp-opensource/kernel_imx/`，你也可直接前往索尼官方页面下载：

> **https://oss.sony.net/Products/Linux/Audio/NW-A105_Ver20211130.html**
>
> 适用型号：NW-A105, NW-A105HN, NW-A106, NW-A106HN, NW-A107,
> NW-A100TPS, NW-ZX505, NW-ZX507

内核源码位于 `sony-gplsource/vendor/nxp-opensource/kernel_imx/`。

---

## 启动问题诊断：为什么早期自定义内核无法启动

在成功编译可启动的内核之前，经历了多次失败。以下是完整的根因分析过程。

### 故障现象

- 设备：Sony NW-A105（`icx1293_002`，Android 9，原厂固件 `4.06.00`）
- 操作：只刷入自编译 `boot.img` + `blank_vbmeta.img`（其他分区未动）
- 结果：设备**最多启动到 bootloader（fastboot 模式）**，内核从未成功加载

### 启动链架构

NXP i.MX8MM 的完整启动链：

```
Boot ROM (SOC 内部固化)
  → SPL (U-Boot Secondary Program Loader)
    → ATF BL31 (ARM Trusted Firmware, EL3)
      → OP-TEE Trusty (安全 OS, 可选)
        → U-Boot Proper (bootloader)
          → Linux Kernel
```

U-Boot 通过 **AVB (Android Verified Boot)** 验证流程加载 Android boot image：

```c
// f_fastboot.c: do_boota() 函数逻辑 (简化)
1. fastboot_get_lock_stat()         // 检查 bootloader 锁定状态
2. avb_ab_flow_fast()               // AVB 验证 boot/dtbo/system 分区
3. android_image_check_header(hdr)  // 检查 "ANDROID!" magic
4. image_arm64(kernel_data)         // 检测 ARM64 PE/COFF 格式
5. // 从 DTBO 分区加载 DTB:
   entry_idx = icx_dmp_board_id.bid; // ← GPIO 读取 Board ID
   dt_entry = get_dt_entry(dt_img, entry_idx);
   memcpy((void*)hdr->second_addr, dt_entry_data, fdt_size); // ← 复制到 second_addr
6. do_booti(kernel_addr, "-", fdt_addr); // 启动 ARM64 内核
```

### 根因 #1：`CONFIG_DM_VERITY` 缺失

**症状**：内核启动，但 Android init 进程无法挂载 system 分区，设备看门狗超时后重启

**原理**：原厂 `system` 分区使用 dm-verity（device-mapper verity）保护。通过从原厂 boot.img 提取的 `/proc/config.gz`（`CONFIG_IKCONFIG=y` 允许运行时读取压缩内核配置）与当前 `.config` 对比发现缺失：

```
原厂 boot.img 内核配置:
  CONFIG_DM_BUFIO=y
  CONFIG_DM_VERITY=y
  CONFIG_DM_VERITY_FEC=y

早期 nwa105_kernel_config:
  (全部缺失 — dm-verity 未启用)
```

提取原厂内核配置的方法：

```python
# 原厂 boot.img 内核嵌入了压缩配置 (CONFIG_IKCONFIG=y)
import struct, gzip
with open('original_image/A105/boot.img', 'rb') as f:
    header = f.read(4096)
    page_size = struct.unpack('<I', header[36:40])[0]
    kernel_size = struct.unpack('<I', header[8:12])[0]
    f.seek(page_size)
    kernel_data = f.read(kernel_size)

# 搜索 IKCFG 标记
ikcfg_st = kernel_data.find(b'IKCFG_ST') + 8
ikcfg_ed = kernel_data.find(b'IKCFG_ED')
config_gz = kernel_data[ikcfg_st:ikcfg_ed]
config_text = gzip.decompress(config_gz)  # → 168,642 字节纯文本配置
```

### 根因 #2：`second_addr=0x00000000` 导致 DTB 加载到地址 0

**症状**：内核在早期启动阶段崩溃（静默，无任何串口输出），设备回退到 fastboot 模式

**原理**：U-Boot `do_boota` 将 DTBO 分区中选中的 DTB 复制到 `hdr->second_addr`：

```c
// f_fastboot.c:2533
memcpy((void *)(ulong)hdr->second_addr,
       (void *)((ulong)dt_img + be32_to_cpu(dt_entry->dt_offset)),
       fdt_size);
```

然后用它作为 `booti` 的 fdt 参数：

```c
sprintf(fdt_addr, "0x%x", hdr->second_addr);
boot_args[3] = fdt_addr;  // booti <kernel> - <fdt>
```

当 `mkbootimg` 未传入 `--second` 参数时，`second_size=0` 且 `second_addr=0x00000000`。DTB 被复制到物理地址 0（ARM64 的异常向量表位置），内核解压阶段立即崩溃。

**原厂值**：`second_addr=0x43400000`（合法 RAM 地址，位于 kernel 和 ramdisk 地址空间之间）

**修复**：传入 `--second` 嵌入一个 DTB 文件（即使实际由 dtbo 分区在运行时加载），`mkbootimg` 根据 `base + second_offset` 自动计算 `second_addr`：

```
second_addr = base + second_offset = 0x40000000 + 0x03400000 = 0x43400000
```

### 根因 #3：内核格式不匹配

**症状**：U-Boot 打印 "bad boot image magic" 或 kernel 检测失败后进入 fastboot 模式

**原理**：U-Boot `do_boota` 中的 `image_arm64()` 函数检查内核是否为 ARM64 PE/COFF 格式：

```c
// f_fastboot.c:2437 — 仅对 ARM64 Image 格式有效
if (image_arm64((void *)((ulong)hdr + hdr->page_size))) {
    memcpy((void *)(long)hdr->kernel_addr,           // 直接复制未压缩内核
            (void *)((ulong)hdr + hdr->page_size),
            hdr->kernel_size);
} else {
    // 尝试 LZ4 解压 (CONFIG_LZ4)
}
```

ARM64 PE/COFF Image 的识别特征：
- 文件偏移 0x00：`MZ` (0x4d 0x5a) — PE/COFF DOS 头
- 文件偏移 0x38：`ARM\x64` (0x41 0x52 0x4d 0x64 0x40 0x00) — ARM64 标识

**结论**：必须使用 `arch/arm64/boot/Image`（未压缩），不能用 `Image.gz` 或 `zImage`。

### 根因 #4：`CONFIG_KPROBES` 缺失

原厂内核启用了 `CONFIG_KPROBES=y` / `CONFIG_KPROBE_EVENTS=y` / `CONFIG_KRETPROBES=y`。KernelSU 的多个 hook 依赖 kprobe 框架。缺失导致 KSU 部分功能异常（但不直接阻止启动）。

### 完整修复清单

| # | 修复 | 类型 | 影响 |
|---|------|------|------|
| 1 | `CONFIG_DM_VERITY=y` + `DM_BUFIO` + `DM_VERITY_FEC` | 配置 | system 分区挂载 |
| 2 | `second_addr=0x43400000` | mkbootimg 参数 | DTB 加载地址 |
| 3 | 使用未压缩 `Image` | 内核格式 | U-Boot 识别 |
| 4 | `CONFIG_KPROBES=y` + `KPROBE_EVENTS` + `KRETPROBES` | 配置 | KSU 正常工作 |
| 5 | `CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y` | 配置 | EAS 调度 |
| 6 | `CONFIG_HZ=250` (原厂值) | 配置 | 调度一致性 |
| 7 | GCC 15 `-Wno-error` 修复 | 编译 | 构建成功 |
| 8 | `DTB_FILE_NAME=sony-imx8mm-icx1293` | 编译参数 | DTB 文件名 |

---

## 内核配置详解

### 配置文件一览

| 文件 | 用途 |
|------|------|
| `sony-gplsource/vendor/nxp-opensource/kernel_imx/.config` | **最终编译用内核配置**（已合并所有补丁） |
| `kernel_patches/nwa105_kernel_config` | USB DAC 项目内核配置（基于 SonyWalkman 稳定版） |
| `SonyWalkman/kernel_imx/walkman.config` | SonyWalkman 项目原始配置（仅作参考） |
| `original_image/A105/boot.img` | 原厂固件 boot.img（提取 ramdisk、cmdline、基地址等参数） |
| `original_image/A105/dtbo.img` | 原厂 DTB 叠加表（6 个硬件变体，与自编译 DTB 逐字节相同） |
| `original_image/A105/blank_vbmeta.img` | 空白 vbmeta（禁用 AVB 验证） |

### 与原厂内核的配置差异

从原厂 `boot.img` 中提取的 `/proc/config.gz`（`CONFIG_IKCONFIG=y`）与当前配置对比：

#### 修复启动失败的关键项（原厂有，早期自定义配置缺失）

| 配置项 | 作用 | 缺失后果 |
|--------|------|----------|
| `CONFIG_DM_VERITY=y` | Android dm-verity 设备映射器 | system 分区无法挂载，init 进程无法启动 |
| `CONFIG_DM_BUFIO=y` | dm-verity 依赖 | dm-verity 无法工作 |
| `CONFIG_DM_VERITY_FEC=y` | dm-verity 前向纠错 | verity 校验失败时无法恢复 |
| `CONFIG_KPROBES=y` | 内核探针框架 | KernelSU 及其他内核模块无法工作 |
| `CONFIG_KPROBE_EVENTS=y` | kprobe 事件追踪 | kprobe 事件不可用 |
| `CONFIG_KRETPROBES=y` | 函数返回探针 | KernelSU 部分 hook 失效 |
| `CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y` | EAS 调度器调频 governor | 能效调度失效 |
| `CONFIG_HZ=250` | 内核定时器频率 | 调度行为与原厂不一致 |

#### USB DAC 新增项

| 配置项 | 值 | 作用 |
|--------|-----|------|
| `CONFIG_USB_CONFIGFS_F_UAC2` | y | USB Audio Class 2.0 gadget（configfs 控制接口） |
| `CONFIG_USB_F_UAC2` | y | UAC2 函数驱动（内核内置，原厂为 =m 模块） |
| `CONFIG_USB_U_AUDIO` | y | USB 音频 gadget 辅助驱动（内核内置） |
| `CONFIG_SND_USB_AUDIO` | y | USB 音频设备侧 ALSA 驱动 |

#### KernelSU 项

| 配置项 | 值 | 作用 |
|--------|-----|------|
| `CONFIG_KSU` | y | KernelSU 内核模块 |

#### 功耗优化项（DAC 播放时降低 CPU 频率）

| 改动 | 文件 | 效果 |
|------|------|------|
| DTS 新增 200/400/600/700/800/1000MHz OPP | `fsl-imx8mm.dtsi` | CPU 可降至 200MHz（原厂最低 1.2GHz） |
| PLL 新增 200/400MHz 分频 | `clk-imx8mm.c` | 为低频率 OPP 提供时钟源 |
| conservative governor 阈值调优 | `cpufreq_conservative.c` | UP: 80→85, DOWN: 20→45, STEP: 5→10 |

### 内核源码修改清单（相对于索尼 GPL 源码）

以下为从索尼 GPL 源码包修改的所有文件。所有改动已合并到 `sony-gplsource/` 目录中。

#### KernelSU 集成

KernelSU 在源码树内作为内核子目录（`KernelSU/`）存在，并通过以下文件 hook 到内核：

| 文件 | 改动 |
|------|------|
| `drivers/Makefile` | `+obj-$(CONFIG_KSU) += kernelsu/` |
| `drivers/Kconfig` | `+source "drivers/kernelsu/Kconfig"` |
| `fs/exec.c` | KSU 进程执行 hook |
| `fs/open.c` | KSU 文件打开 hook |
| `fs/read_write.c` | KSU 读写操作 hook |
| `fs/stat.c` | KSU stat 操作 hook |
| `drivers/input/input.c` | KSU 输入事件 hook（按键拦截） |
| `security/selinux/hooks.c` | KSU SELinux LSM hook |
| `security/selinux/selinuxfs.c` | KSU SELinux 文件系统接口 |
| `security/selinux/include/classmap.h` | SELinux 安全类表（GCC 15 修复） |
| `KernelSU/` (309 文件) | KSU 核心源码（内核模块 + ksud + 用户态 API） |
| `drivers/kernelsu/` | KSU 内核驱动（Makefile + Kconfig + 源码） |

KernelSU 通过 `CONFIG_KSU=y` 配置项编译进内核，不依赖独立模块加载。系统属性中的 `ro.product.model=NW-A100Series` 标识设备型号，KSU Manager App 据此识别。

#### USB DAC / 功耗优化

| 文件 | 改动 | 作用 |
|------|------|------|
| `arch/arm64/boot/dts/freescale/fsl-imx8mm.dtsi` | 新增 6 个低频 OPP | CPU 可降至 200MHz |
| `drivers/clk/imx/clk-imx8mm.c` | 新增 ARM PLL @ 200/400MHz | 低频 OPP 时钟源 |
| `drivers/cpufreq/cpufreq_conservative.c` | 阈值调优 | 更积极降频，更保守升频 |

**注意**：USB UAC2 驱动本身 (`drivers/usb/gadget/function/f_uac2.c`, `u_audio.c`) 为标准内核源码，无需修改。只需在 `.config` 中启用即可。

#### GCC 15 / 构建系统修复

| 文件 | 改动 | 作用 |
|------|------|------|
| `Makefile` (顶层) | `KBUILD_CFLAGS += -Wno-error=...` 4 行 | GCC 15 警告降级 |
| `drivers/mxc/gpu-viv/Kbuild` | `-Werror → -Wno-error` | GPU 驱动可编译 |
| `drivers/net/wireless/qcacld-2.0_sony/Kbuild` | 移除 `-Werror` | WiFi 驱动可编译 |
| `scripts/dtc/dtc-lexer.lex.c_shipped` | `extern YYLTYPE yylloc` | DTC lexer 修复 |
| `scripts/selinux/genheaders/genheaders.c` | SELinux 头生成器修复 | GCC 15 兼容 |
| `scripts/selinux/mdp/mdp.c` | SELinux MDP 工具修复 | GCC 15 兼容 |

### 完整配置差异（当前 .config vs 原厂 config）

从原厂 boot.img 提取的 `/proc/config.gz` (168,642 bytes) 与当前 `.config` 的完整对比：

```
# ===== 当前额外启用 (=y) =====
CONFIG_DM_BUFIO=y             # dm-verity 依赖（boot fix）
CONFIG_DM_VERITY=y            # Android system 分区挂载（boot fix）
CONFIG_DM_VERITY_FEC=y        # verity 前向纠错（boot fix）
CONFIG_KPROBES=y              # 内核探针（boot fix）
CONFIG_KPROBE_EVENTS=y        # kprobe 事件（boot fix）
CONFIG_KRETPROBES=y           # 函数返回探针（boot fix）
CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y  # EAS 调频 governor（boot fix）
CONFIG_KSU=y                  # KernelSU
CONFIG_USB_CONFIGFS_F_UAC2=y  # USB DAC gadget（项目核心）
CONFIG_USB_F_UAC2=y           # UAC2 函数驱动（原厂=m）
CONFIG_USB_U_AUDIO=y          # USB 音频辅助（原厂=m）
CONFIG_USB_AUDIO=m            # USB 音频 gadget legacy（原厂=m）
CONFIG_OVERLAY_FS=y           # OverlayFS（原厂=m）
CONFIG_EXPORTFS=y             # NFS export（原厂=m）

# ===== 原厂有但当前未启用 =====
CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND=y  # 当前使用 conservative
CONFIG_PRIMA_WLAN_11AC_HIGH_TP=y        # WiFi 高吞吐（当前未启用）
CONFIG_PRIMA_WLAN_OKC=y                 # WiFi OKC（当前未启用）
CONFIG_QCA_CLD_WLAN_SONY=m              # WiFi 驱动（当前未启用）
CONFIG_QCOM_TDLS=y                      # WiFi TDLS（当前未启用）
CONFIG_WLAN_FEATURE_11W=y               # WiFi 11W（当前未启用）
CONFIG_WLAN_FEATURE_LPSS=y              # WiFi LPSS（当前未启用）
CONFIG_WLAN_FEATURE_NAN=y               # WiFi NAN（当前未启用）
CONFIG_WLAN_FEATURE_NAN_DATAPATH=y      # WiFi NAN 数据路径（当前未启用）
CONFIG_WLAN_OFFLOAD_PACKETS=y           # WiFi 卸载（当前未启用）
CONFIG_WLAN_SYNC_TSF=y                  # WiFi TSF 同步（当前未启用）

# ===== 定时器频率 =====
# 原厂: CONFIG_HZ=250
# 当前: CONFIG_HZ=300 (SonyWalkman 项目使用，DAC 场景需要更高的调度精度)
```

**WiFi 驱动未启用说明**：当前配置主要为 USB DAC 使用场景优化。WiFi 驱动 (`qca_cld_wlan_sony`) 在需要时可通过 `make menuconfig` 重新启用，不影响 USB DAC 功能。

### boot.img 参数来源与验证

以下参数从原厂 `boot.img`（`original_image/A105/boot.img`）头部提取并经 `mkbootimg` 验证：

| 参数 | 值 | 来源 |
|------|-----|------|
| Kernel 加载地址 | `0x40480000` | 原厂 boot.img header `kernel_addr` |
| Ramdisk 加载地址 | `0x43600000` | 原厂 boot.img header `ramdisk_addr` |
| Second (DTB) 地址 | `0x43400000` | 原厂 boot.img header `second_addr` |
| Tags 地址 | `0x40400100` | 原厂 boot.img header `tags_addr` |
| Base | `0x40000000` | `tags_addr - 0x100`（标准 Android 约定） |
| Page size | `2048` | 原厂 boot.img header |
| Header version | `1` | 原厂 boot.img header（Android 9+ A/B 设备标准） |
| Kernel 格式 | ARM64 未压缩 PE/COFF Image | U-Boot `booti` 命令要求 |

**⚠️ `second_addr` 必须为 `0x43400000` 非零值**：U-Boot 的 `do_boota` 函数通过 `hdr->second_addr` 作为 DTB 加载目标地址。
早期编译的 boot.img 因 `mkbootimg` 在未传入 `--second` 参数时将 `second_addr` 置零，导致 DTB 被复制到地址 0 而无法启动。
修复方法是传入 `--second` 嵌入一个 DTB 文件（即使实际由 dtbo 分区提供），强制 `second_addr` 为非零值。

### 原厂 cmdline

```
init=/init androidboot.console=ttymxc1 androidboot.hardware=icx1293
cma=800M@0x400M-0xb7fM androidboot.primary_display=imx-drm
firmware_class.path=/vendor/firmware transparent_hugepage=never
androidboot.displaymode=720p buildvariant=user
```

### DTB 变体说明

设备使用 **dtbo 分区** 而非 boot.img 内嵌 DTB。dtbo.img 包含 6 个 DTB 变体（`icx1293-0` ~ `icx1293-5`），U-Boot 通过 GPIO 读取 Board ID 选择对应条目：

| 变体 | Board ID | eMMC 配置差异 |
|------|----------|---------------|
| 0 | BB (调试板) | `fsl,strobe-dll-delay-target = <9>` |
| 1 | LF (量产) | `max-frequency-limit = <50000000>` |
| 2 | LF | 按 eMMC 厂商设置驱动强度 (Toshiba/Hynix/Samsung) |
| 3-5 | 后续版本 | emmc-drive-strength + 200MHz pinmux |

当前编译的 6 个 DTB 与原厂 dtbo.img 中对应条目**逐字节相同**。

### 硬件平台详情

| 属性 | 值 |
|------|-----|
| SoC | NXP i.MX8M Mini (i.MX8MM)，4× Cortex-A53 @ 1.8GHz + 1× Cortex-M4 |
| 内核版本 | Linux 4.14.78 |
| 架构 | ARM64 (AArch64)，4KB 页面，48-bit VA |
| RAM | 2GB LPDDR4 (0x40000000-0xC0000000) |
| eMMC | 内嵌存储 (`/dev/block/mmcblk2`)，通过 USDHC3 控制器 |
| SD 卡 | 外部存储 (`/dev/block/mmcblk1`)，通过 USDHC2 控制器 |
| PMIC | Rohm BD71847 (I2C 地址 0x4B) |
| 充电器 | TI BQ25898 (I2C 地址 0x6B) |
| 电量计 | Maxim MAX1704x (I2C 地址 0x36) |
| DAC | CXD3778GF (通过 SAI 接口连接) |
| 音频 | AK4497 (高解析度 DAC)、AK4458 (标准 DAC) |
| 显示 | Himax HX83102D MIPI-DSI LCD (720×1280) |
| 触控 | 电容式触摸屏 |
| Wi-Fi / BT | Qualcomm QCA CLD (Rome 系列，UART 接口) |
| USB | 双路 USB-C (USB 2.0 OTG)，支持 Type-C PD |
| 调试串口 | UART2 (ttymxc1)，GPIO5_IO24 (RXD) / GPIO5_IO25 (TXD)，115200 8N1 |
| Android | Android 9 (SDK 28)，system-as-root，A/B 分区，AVB 验证 |

**⚠️ 调试串口注意事项**：量产固件 (`buildvariant=user`) 在 `board_init()` 中将 UART2 引脚重新配置为 GPIO 输出模式（`board_disable_console()` 函数），导致无法通过串口观察启动日志。如需调试，使用 `eng` 构建或通过 ADB 查看 `dmesg`。

### Board ID 读取机制

U-Boot 通过 `CONFIG_ICX_DMP_BOARD_ID` 驱动读取 5 个 GPIO 引脚来识别硬件版本：

```
GPIO 引脚             读取值        含义
────────────────────────────────────────────
GPIO4_IO10 (bid1)  ─┐
GPIO4_IO11 (bid2)   ├─ Board ID   0x0=BB(调试), 0x1=LF(量产), 0x2+=后续
GPIO4_IO1  (bid3)  ─┘
GPIO1_IO5  (sid0)  ─── SID0       三态输入 (低/高/开路)
GPIO1_IO15 (sid1)  ─── SID1       ICX1293=0, ICX1295=1
```

映射逻辑（`drivers/misc/icx_dmp_board_id.c`）：

```
sid1=0, sid0=2 → ICX1293 (NW-A100 系列)
sid1=1, sid0=2 → ICX1295 (NW-ZX500 系列)
```

`bid` 值直接作为 DTBO 分区中 DTB 表的索引（`f_fastboot.c:2527`）：

```c
entry_idx = icx_dmp_board_id.bid;
dt_entry = get_dt_entry(dt_img, entry_idx);
```

这意味着 **Board ID 决定加载哪个 DTB 变体**。如果 bid 值和对应 DTB 的 eMMC 配置与实际硬件不匹配，内核在初始化 eMMC 时挂死。

### 原厂 boot.img 完整解析

通过解析 Android boot image 二进制头部（`ANDROID!` magic），获得原厂固件的所有参数：

```
字段            偏移  值                        说明
──────────────────────────────────────────────────────
magic           0     "ANDROID!"                 Android boot image 标识
kernel_size     8     28,940,800 (27.6 MB)       未压缩 ARM64 Image
kernel_addr     12    0x40480000                 内核加载地址
ramdisk_size    16    12,622,712 (12.0 MB 压缩)  gzip 压缩 cpio
ramdisk_addr    20    0x43600000                 Ramdisk 加载地址
second_size     24    0                          第二级 bootloader 大小
second_addr     28    0x43400000                 DTB 加载地址（关键！）
tags_addr       32    0x40400100                 ATAGS / 设备树地址
page_size       36    2048                       页面大小
header_version  40    1                          v1 格式（支持 recovery_dtbo）
os_version      44    301990235                  Android 9 (SDK 28)
name            48    ""                         空名称
cmdline         64    512 字节内核命令行
```

**Ramdisk 内容** (解压后 27.0 MB)：
- `init` — Android recovery init 进程
- `init.rc` / `init.recovery.icx1293.*.rc` — recovery 启动脚本
- `etc/recovery.fstab` — 分区表（system, userdata, misc, zram）
- `sbin/` — recovery 工具（adbd, e2fsdroid, mke2fs, update_engine_sideload）
- `sepolicy` — SELinux 策略
- `plat_file_contexts` / `plat_property_contexts` — SELinux 文件/属性上下文
- `prop.default` — 默认系统属性（含 `ro.product.model=NW-A100Series`）
- `lib/` — ARM32 动态链接器及其库（`ld-linux-armhf.so.3` 和 libc/libm/libcrypto/libssl 等）



---

## 从源码构建内核（完整流程）

### 环境要求

| 组件 | 版本 / 说明 |
|------|-------------|
| 操作系统 | Ubuntu 20.04+ / Debian 11+（WSL2 可用） |
| 交叉编译器 | `aarch64-linux-gnu-gcc`（GCC 11~15 均支持，已内置 GCC 15 兼容修复） |
| 依赖包 | `build-essential flex bison bc libssl-dev device-tree-compiler gcc-aarch64-linux-gnu` |
| 打包工具 | `mkbootimg`（`sudo apt install mkbootimg`） |
| 原厂固件 | `original_image/A105/boot.img`（提取 ramdisk 和参数） |

安装依赖：

```bash
sudo apt update
sudo apt install -y build-essential flex bison bc libssl-dev \
    device-tree-compiler gcc-aarch64-linux-gnu mkbootimg cpio
```

### 第一步：确认源码和补丁已就绪

本仓库已包含所有必要的源码和补丁，无需额外下载：

```bash
cd /home/he0xd4c0/A100_ZX500_USB-DAC

# 确认内核源码存在
ls sony-gplsource/vendor/nxp-opensource/kernel_imx/Makefile

# 确认 KSU 已集成
ls sony-gplsource/vendor/nxp-opensource/kernel_imx/KernelSU/

# 确认补丁已应用（检查关键文件）
grep "CONFIG_KSU" sony-gplsource/vendor/nxp-opensource/kernel_imx/drivers/Makefile
# 预期输出: obj-$(CONFIG_KSU) += kernelsu/
```

### 第二步：确认内核配置

当前 `.config` 已包含所有必需选项。验证关键配置：

```bash
cd sony-gplsource/vendor/nxp-opensource/kernel_imx/

# 验证启动必需项
grep -E "CONFIG_DM_VERITY=y|CONFIG_KPROBES=y|CONFIG_HZ=250" .config

# 验证 USB DAC 项
grep "CONFIG_USB_CONFIGFS_F_UAC2=y" .config

# 验证 KernelSU
grep "CONFIG_KSU=y" .config
```

如需重新生成配置（例如从 `kernel_patches/nwa105_kernel_config` 开始），追加缺失项：

```bash
cp ../../kernel_patches/nwa105_kernel_config .config

# 追加启动修复（原厂要求）
echo "CONFIG_DM_BUFIO=y" >> .config
echo "CONFIG_DM_VERITY=y" >> .config
echo "CONFIG_DM_VERITY_FEC=y" >> .config
echo "CONFIG_DM_VERITY_HASH_PREFETCH_MIN_SIZE=1" >> .config
echo "CONFIG_KPROBES=y" >> .config
echo "CONFIG_KPROBE_EVENTS=y" >> .config
echo "CONFIG_KRETPROBES=y" >> .config
echo "CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y" >> .config

# 启用 USB DAC gadget configfs 接口
sed -i 's/# CONFIG_USB_CONFIGFS_F_UAC2 is not set/CONFIG_USB_CONFIGFS_F_UAC2=y/' .config

# 解析依赖
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make olddefconfig
```

### 第三步：编译内核

```bash
cd sony-gplsource/vendor/nxp-opensource/kernel_imx/

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export DTB_FILE_NAME=sony-imx8mm-icx1293   # 必须！否则 DTB 构建失败

# 编译（首次需 make clean 清除旧构建产物）
make clean 2>/dev/null || true
make -j$(nproc)
```

产物：
- `arch/arm64/boot/Image` — 内核镜像（ARM64 PE/COFF，约 31MB）
- `arch/arm64/boot/dts/sony/sony-imx8mm-icx1293-{0..5}.dtb` — 6 个 DTB 变体

**GCC 15 编译说明**：内核 `Makefile` 和 `drivers/mxc/gpu-viv/Kbuild` 已内置 GCC 15 兼容性修复（`-Wno-error=incompatible-pointer-types` 等）。
如果使用 GCC 13 以下版本编译，这些额外参数会被 `cc-option` 自动跳过，不影响构建。

### GCC 15 编译错误修复详情

Linux 4.14.78 (2018) 未适配 GCC 15 (2025)。以下是编译时遇到的具体错误及修复方法：

#### 错误 1：`-Werror=incompatible-pointer-types`

**文件**：`sound/soc/fsl/fsl_rpmsg_i2s.c:155`、`imx-pcm-dma-v2.c:95,285`、`drivers/misc/lifmd6000/lifmd6000.c:549`

**原因**：GCC 15 对函数指针类型检查更严格。Kernel 的 `test_and_set_bit` / `schedule_delayed_work` 等内联函数在 GCC 15 下参数类型不匹配。

**修复**：在顶层 `Makefile` 第 906 行后追加：
```makefile
KBUILD_CFLAGS += $(call cc-option,-Wno-error=incompatible-pointer-types)
```

#### 错误 2：`-Werror=enum-int-mismatch`

**文件**：`drivers/mxc/gpu-viv/hal/os/linux/kernel/gc_hal_kernel_os.c:6072`

**原因**：`_QuerySignal` 函数声明返回 `gceSTATUS`（enum），但调用点期望 `int`。GCC 15 默认将此视为错误。

**修复**：
```makefile
KBUILD_CFLAGS += $(call cc-option,-Wno-error=enum-int-mismatch)
```

#### 错误 3：GPU 驱动 `EXTRA_CFLAGS += -Werror`

**文件**：`drivers/mxc/gpu-viv/Kbuild:82`

**原因**：GPU 闭源驱动（Vivante GCxxx）硬编码 `-Werror`，在 GCC 15 下所有新增警告均变为错误。

**修复**：`EXTRA_CFLAGS += -Wno-error`（弱化所有警告，不阻止构建）

#### 错误 4：`-Werror=address`

**文件**：`drivers/mxc/gpu-viv/hal/kernel/gc_hal_kernel_db.c:1238`

**原因**：代码比较 `&list == NULL`（取地址后判空），GCC 15 认为此比较恒为真。

**修复**：
```makefile
KBUILD_CFLAGS += $(call cc-option,-Wno-error=address)
```

#### 错误 5：`-Werror=array-bounds`

**文件**：`drivers/mxc/gpu-viv/hal/os/linux/kernel/gc_hal_kernel_debug.h:132`

**原因**：GCC 15 对 `va_list` 的数组边界检查更严格。Vivante 驱动的可变参数调试宏触发此警告。

**修复**：
```makefile
KBUILD_CFLAGS += $(call cc-option,-Wno-error=array-bounds)
```

#### 错误 6：SELinux `classmap.h` PF_MAX 越界

**文件**：`security/selinux/include/classmap.h`

**原因**：GCC 15 新增 `PF_XDP`(44) 和 `PF_MCTP`(45) socket 协议族。SELinux 的 `classmap.h` 中 `PF_MAX > 44` 检查不足，导致编译时数组越界警告。

**修复**（已提交到 git）：
```c
// 在 security/selinux/include/classmap.h 的安全类表中新增：
{ "xdp_socket", { COMMON_SOCK_PERMS, NULL } },
{ "mctp_socket", { COMMON_SOCK_PERMS, NULL } },
// 修改宏:
#define PF_MAX 46  // was 44
```

#### 错误 7：DTC lexer `yylloc` 重复定义

**文件**：`scripts/dtc/dtc-lexer.lex.c_shipped`

**原因**：GCC 15 不再允许全局变量的重复 tentative definition。`yylloc` 在多个编译单元中重复定义。

**修复**：
```c
extern YYLTYPE yylloc;  // 改为 extern 声明（仅一处定义）
```



### 第四步：打包 boot.img

```bash
# 提取原厂 ramdisk
python3 -c "
import struct
with open('original_image/A105/boot.img', 'rb') as f:
    header = f.read(4096)
    page_size = struct.unpack('<I', header[36:40])[0]
    kernel_size = struct.unpack('<I', header[8:12])[0]
    ramdisk_size = struct.unpack('<I', header[16:20])[0]
    ramdisk_offset = page_size + ((kernel_size + page_size - 1) // page_size) * page_size
    f.seek(ramdisk_offset)
    ramdisk_data = f.read(ramdisk_size)
with open('/tmp/orig_ramdisk.img', 'wb') as out:
    out.write(ramdisk_data)
print(f'Ramdisk: {len(ramdisk_data)} bytes')
"

# 打包 boot.img
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
    --output output/boot.img
```

**打包参数说明**：

- `--kernel`: 必须是未压缩的 `Image`（ARM64 PE/COFF 格式，`MZ` 文件头），**不能是 `Image.gz`**。U-Boot 通过 `booti` 命令启动 ARM64 内核，`image_arm64()` 检测 PE/COFF 特征字节 `ARM\x64`。
- `--ramdisk`: 必须从原厂 boot.img 提取。即使正常模式下 U-Boot 因 `CONFIG_SYSTEM_RAMDISK_SUPPORT` 不传 ramdisk 给内核，boot.img header 中的 `ramdisk_size` 和 `ramdisk_addr` 字段仍需有效值。
- `--second`: 传入一个 DTB 文件。虽然实际 DTB 由 `dtbo` 分区在运行时加载，但此参数确保 `second_addr=0x43400000` 非零（U-Boot 将 DTB 从 dtbo 复制到此地址）。
- `--base` 和 `*_offset`: 所有地址必须与原厂 boot.img header 一致，否则 U-Boot 加载位置错误导致启动失败。

验证产物：

```bash
file output/boot.img
# 预期: Android bootimg, kernel (0x40480000), ramdisk (0x43600000),
#        second stage (0x43400000), page size: 2048, cmdline (...)
```

### 第五步：刷入设备

```bash
adb reboot bootloader

# 刷入自编译 boot.img
fastboot flash boot output/boot.img

# 刷入 dtbo（自编译 DTB 与原厂逐字节相同，也可直接用原厂 dtbo.img）
fastboot flash dtbo original_image/A105/dtbo.img

# 禁用 AVB 验证
fastboot flash vbmeta original_image/A105/blank_vbmeta.img

fastboot reboot
```

### 验证

```bash
# 确认 USB DAC 驱动已加载
adb shell "zcat /proc/config.gz | grep UAC2"
# 预期: CONFIG_USB_CONFIGFS_F_UAC2=y

# 确认 KernelSU 工作
adb shell "su -c 'echo ok'"
# 预期: ok

# 查看内核版本
adb shell "uname -r"
# 预期: 4.14.78-xxx (含 -g 后缀的 git commit hash)
```

---

## 工作原理

### 内核层：USB Gadget UAC2 驱动

设备通过 **USB Gadget 框架** 的 `f_uac2`（Function USB Audio Class 2.0）将自身枚举为 USB 声卡。

**关键内核配置与模块**：

```
CONFIG_USB_CONFIGFS_F_UAC2=y    ← configfs 用户态控制接口 (必需)
    ├── select USB_U_AUDIO=y    ← USB 音频辅助层 (u_audio.c)
    └── select USB_F_UAC2=y     ← UAC2 函数驱动 (f_uac2.c)
```

**数据流向**：

```
USB 总线 (PC→设备)
    ↓
ci_hdrc.0 (ChipIdea 高速 USB 控制器, drivers/usb/chipidea/)
    ↓
UDC (USB Device Controller) → USB Gadget 层
    ↓
f_uac2.c (USB Audio Class 2.0 函数驱动)
    ↓ ALSA capture (card#2, pcmC2D0c)
uac2_bridge daemon (用户态, mmap PCM)
    ↓ ALSA playback (card#1, pcmC1D0p)
imx-audio-cxd3778gf (ASoC 机器驱动, sound/soc/fsl/imx-audio-cxd3778gf.c)
    ↓ SAI (Synchronous Audio Interface)
CXD3778GF DAC → 耳机放大器 → 耳机孔
```

**USB 枚举参数** (由 App 通过 configfs 设置)：

```
p_chmask=3          (立体声, 左右声道)
p_srate=192000      (最高 192 kHz)
p_ssize=4           (24-bit, S24_3LE 格式)
c_chmask=3          (capture 同样立体声)
c_srate=192000
c_ssize=4
```

### 用户态：configfs 控制流程

App 通过写入 `/config/usb_gadget/` 下的 configfs 文件系统控制 USB 功能：

```bash
# 1. 创建 UAC2 函数实例
mkdir /config/usb_gadget/g1/functions/uac2.gs0

# 2. 配置音频参数
echo 3 > /config/usb_gadget/g1/functions/uac2.gs0/p_chmask    # 立体声
echo 192000 > /config/usb_gadget/g1/functions/uac2.gs0/p_srate
echo 4 > /config/usb_gadget/g1/functions/uac2.gs0/p_ssize     # 24-bit

# 3. 绑定到 USB 配置并触发重枚举
ln -s /config/usb_gadget/g1/functions/uac2.gs0 \
      /config/usb_gadget/g1/configs/b.1/f1

# 4. 触发 USB 重枚举 (PC 识别新设备)
echo "" > /config/usb_gadget/g1/UDC
echo "ci_hdrc.0" > /config/usb_gadget/g1/UDC
```

### Bridge Daemon：PCM 数据桥接

`uac2_bridge` 是一个静态链接的 arm64 ELF（基于 tinyalsa）：

```c
// 伪代码逻辑
struct pcm *cap = pcm_open(2, 0, PCM_IN,  &config);   // card#2 capture (USB)
struct pcm *out = pcm_open(1, 0, PCM_OUT, &config);   // card#1 playback (DAC)

// 自动检测 USB 协商参数 (pcm_get_config)
unsigned int rate, channels, bits;
pcm_get_config(cap, &rate, &channels, &bits);

// 循环：从 USB capture 读取，写入 DAC playback
while (running) {
    pcm_read(cap, buffer, frames);
    pcm_write(out, buffer, frames);
}
```

守护进程编译依赖 `tinyalsa`（`git submodule update --init`）：

```
daemon/uac2_bridge.c
    │  #include <tinyalsa/asoundlib.h>
    ▼
daemon/tinyalsa/src/pcm.c      ← PCM 设备操作 (open/read/write/mmap)
daemon/tinyalsa/src/mixer.c    ← ALSA 混音器控制
```

最终生成静态链接的 `uac2_bridge`（~200KB），内置于 App assets 中，无需安装额外库。

### 功耗优化：CPU 低频率支持

原厂内核 CPU 频率范围为 **1.2GHz ~ 1.8GHz**。USB DAC 模式下的典型负载（PCM 桥接 + ALSA）仅需很低的 CPU 占用。

通过 DTS 和时钟驱动补丁，新增了 **200/400/600/700/800/1000MHz** 六个低频 OPP：

```dts
// fsl-imx8mm.dtsi: arm_opp_table 新增条目
opp-200000000  { opp-hz = /bits/ 64 <200000000>;  opp-microvolt = <800000>; };
opp-400000000  { opp-hz = /bits/ 64 <400000000>;  opp-microvolt = <800000>; };
// ... 600/700/800/1000MHz
```

对应的时钟 PLL 分频（`clk-imx8mm.c`）：

```c
PLL_1416X_RATE(400000000U, 200, 3, 2),  // ARM PLL → 400MHz
PLL_1416X_RATE(200000000U, 200, 3, 3),  // ARM PLL → 200MHz
```

配合 conservative governor 阈值调优（更积极地降频）：

```c
#define DEF_FREQUENCY_UP_THRESHOLD    (85)  // 原 80 → 85 (不轻易升频)
#define DEF_FREQUENCY_DOWN_THRESHOLD  (45)  // 原 20 → 45 (更积极降频)
#define DEF_FREQUENCY_STEP            (10)  // 原 5  → 10 (更大的频率调整步长)
```

实际效果：DAC 播放时 CPU 稳定在 400~800MHz，功耗降低约 30-50%。



---

## 常见问题

**PC 没有识别到声卡**
- 确认 A105 已连接 ADB（`adb devices`）
- 查看 App 状态栏，确认 Bridge 状态为"运行中"
- 尝试关闭"保留 ADB"选项后重新开启 DAC

**声音有爆音/杂音**
- 切换到"低功耗 PCM 输出"模式
- 降低 PC 端播放软件的缓冲区

**App 显示"守护进程启动失败"**
- 通过 ADB 查看日志：`adb shell cat /data/local/tmp/uac2_bridge.log`
- 确认 `pcmC1D0p` 未被 audioserver 占用：`adb shell cat /proc/asound/card1/pcm0p/sub0/status`

---

## 许可证

| 组件 | 许可证 |
|------|--------|
| 内核补丁（`kernel_patches/`） | GPL v2 |
| Bridge 守护进程（`daemon/uac2_bridge.c`） | GPL v2 |
| tinyalsa（`daemon/tinyalsa/`） | BSD 3-Clause |
| Android App（`activity/`） | MIT |
| 构建脚本（`build-kernel.sh`, `build-bootimg.sh`） | GPL v3 |

### GPL 合规声明

完整的 GPL 合规信息（Sony 原始源码来源、内核修改内容、KernelSU 声明、
许可证清单）见 **[GPL_COMPLIANCE.md](GPL_COMPLIANCE.md)**。

### GPL 源码

本项目中 `boot.img`（预编译内核）基于索尼发布的 GPL 源码编译并修改。
索尼 GPL 源码已包含在 `sony-gplsource/` 目录下进入版本控制，以履行
GPL 协议的源码分发义务。完整的 GPL 源码包也可从索尼官方下载：

> **https://oss.sony.net/Products/Linux/Audio/NW-A105_Ver20211130.html**

索尼依法必须为使用 GPL/LGPL 代码的产品提供对应源码，该页面由索尼集团运营维护。下载前请阅读索尼的 [OSS 使用须知](https://oss.sony.net/Products/Linux/notice.html)。
