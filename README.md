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
├── Image.gz              # 已编译的内核镜像（含 UAC2 驱动）
├── PLAN.md               # 详细技术方案
├── activity/             # Android 控制 App（Jetpack Compose）
│   └── app/src/main/
│       ├── assets/uac2_bridge   # 已编译的桥接守护进程（arm64 静态）
│       └── java/.../MainActivity.kt
├── daemon/
│   ├── uac2_bridge.c     # 桥接守护进程源码
│   └── tinyalsa/         # 依赖（git submodule）
└── kernel_patches/       # 内核修改记录
```

---

## 部署步骤

### 第一步：刷入内核

> 内核已编译完毕并存放在根目录 `Image.gz`，内置了 UAC2 驱动（`CONFIG_USB_CONFIGFS_F_UAC2=y`）。

```bash
# 重启到 Bootloader 模式
adb reboot bootloader

# 刷入内核（保留 system/vendor 分区不变）
fastboot flash boot Image.gz

# 重启
fastboot reboot
```

验证内核是否包含 UAC2 支持：

```bash
adb shell "zcat /proc/config.gz | grep UAC2"
# 预期输出: CONFIG_USB_CONFIGFS_F_UAC2=y
```

### 第二步：安装控制 App

用 Android Studio 打开 `activity/` 目录，然后直接 **Run → Run 'app'** 安装到设备。

App 内置了已编译好的 `uac2_bridge` 守护进程，**无需手动推送任何文件**。

### 第三步：授予 Root 权限

首次打开 App 时，Magisk / KernelSU 会弹出授权弹窗，选择"授权"即可。

### 第四步：使用

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

### 编译内核

```bash
# 在 WSL Ubuntu 中执行，内核源码位于索尼 GPL 包
cd /home/<user>/nwa100_src_v2/vendor/nxp-opensource/kernel_imx/

# 初始化配置（基于索尼 defconfig）
make ARCH=arm64 android_dmp1_defconfig
echo "CONFIG_USB_CONFIGFS_F_UAC2=y" >> .config
make ARCH=arm64 olddefconfig

# 编译
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
     KCFLAGS='-w' -j$(nproc) Image.gz modules

# 将内核镜像复制到项目根目录
cp arch/arm64/boot/Image.gz /path/to/A105_DAC_Mode/Image.gz
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

- 内核补丁（`kernel_patches/`）：GPL v2
- Bridge 守护进程（`daemon/uac2_bridge.c`）：GPL v2
- tinyalsa（`daemon/tinyalsa/`）：BSD 3-Clause
- Android App（`activity/`）：MIT
