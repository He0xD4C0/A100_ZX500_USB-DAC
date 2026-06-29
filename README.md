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

### 编译内核

```bash
# 在 WSL Ubuntu 中执行
# 自动使用 sony-gplsource/ 下的内核源码编译
./build-kernel.sh

# 也可指定自定义内核源码路径:
# ./build-kernel.sh /custom/path/to/kernel_imx
```

脚本会自动处理 GCC 15 工具链兼容性问题（SELinux classmap.h、dtc yylloc）。

### 打包 boot.img

```bash
# 自动从 original_image/A105/boot.img 提取原厂 ramdisk 并打包
./build-bootimg.sh

# 也可指定自定义原厂 boot.img:
# ./build-bootimg.sh /path/to/stock_boot.img
```

---

## 工作原理

1. **内核层**：启用 `f_uac2`（USB Audio Class 2 Gadget 驱动），让 A105 在连接 PC 时以 USB 声卡身份枚举。PC 发来的 PCM 数据流会出现在 ALSA `card#2` 的 capture 接口上。

2. **configfs**：App 通过 `su` 写入 `/config/usb_gadget/g1/functions/uac2.gs0/`，声明设备能力并将 UAC2 函数绑定到现有 USB 配置，再触发一次 USB 重枚举让 PC 识别新声卡。

3. **Bridge Daemon**：`uac2_bridge` 是一个静态链接的 arm64 可执行文件（基于 tinyalsa），从 `card#2 capture` 读取 PCM 数据，实时写入 `card#1 playback`（CXD3778GF）。启动时自动等待 PC 连接并读取协商好的采样率/位深/声道数，无需硬编码。

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
