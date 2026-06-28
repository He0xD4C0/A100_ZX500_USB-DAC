# NW-A105 USB DAC 模式 — 计划书

**版本:** v2.0  
**最后更新:** 2026-06-28  

使 Sony NW-A105 (icx1293, 内核 4.14.78, Android 9) 具备可开关的 USB DAC 功能——PC/Mac/Android 设备通过 USB 将音频流发送至 A105，由 A105 的 CXD3778GF DAC 解码输出。

## 总体架构

```
┌──────────┐  USB UAC2   ┌───────────────────────────────────┐
│  PC/Mac  │─────────────│  NW-A105 (ci_hdrc.0 gadget 模式)  │
│  (Host)  │  PCM 数据流  │                                   │
└──────────┘              │  f_uac2.ko → ALSA card #2 (capture)│
                          │       ↓ (tinyalsa bridge daemon)   │
                          │  card #1: cxd3778gf (playback)      │
                          │       ↓                             │
                          │  SAI → CXD3778GF DAC → 耳机孔       │
                          └───────────────────────────────────┘
```

### 设计原则

- **不修改索尼 HAL 或闭源模块**
- **纯软件方案**——不需要硬件改动
- **可动态开关**——用户可通过脚本随时启用/禁用
- **用桥接 Daemon**——不从底层篡改音频路由逻辑

---

## 设备信息

```
硬件型号: icx1293_002 (NW-A105)
Android: 9 (API 28)
内核: 4.14.78 #30 PREEMPT
Root: KernelSU

音频拓扑:
  card0: imx-audio-micfil      — 麦克风 (capture only, 8CH)
  card1: imx-audio-cxd3778gf   — 主音频
    ├── pcmC1D0p → hires-out (Hi-Res 播放)
    ├── pcmC1D1p → standard (标准播放)
    ├── pcmC1D1c → standard (标准录音)
    └── pcmC1D2p → hires-out-low-power (低功耗 Hi-Res)

USB 控制器: ci_hdrc.0 (ChipIdea, Device 模式)
Type-C: fusb303d (支持 data_role: host [device], power_role: source [sink])
当前 gadget: idVendor=0x054C, idProduct=0x0CF2, function=ADB
```

---

## GPL 源码位置 (WSL Ubuntu)

```
/home/he0xd4c0/nwa100_src_v2/
├── vendor/nxp-opensource/kernel_imx/    ← 内核源码 (4.14.78)
│   ├── arch/arm64/configs/
│   │   └── android_dmp1_defconfig       ← 索尼使用的 defconfig
│   ├── drivers/usb/gadget/function/
│   │   ├── f_uac2.c                     ← UAC2 gadget (完整)
│   │   ├── u_audio.c                    ← ALSA PCM 层
│   │   ├── f_uac1.c, f_audio_source.c  ← 遗留音频函数
│   │   └── ...
│   └── Makefile
├── prebuilts/                           ← (不含交叉编译器)
├── vendor/sony/
│   ├── apps/HighResMediaPlayer/         ← 高分辨率播放器 (libusb + JNI)
│   ├── external/alsa-lib/               ← ALSA 用户态库 (LGPL)
│   ├── external/alsa-utils/             ← ALSA 工具集 (GPL)
│   └── tools/systemtools/               ← busybox + i2c-tools
└── ...
```

WSL 网络路径: `\\\\wsl.localhost\\Ubuntu\\home\\he0xd4c0\\nwa100_src_v2\\`

---

## 内核编译

### 交叉编译器

使用 WSL apt 安装的 `aarch64-linux-gnu-` GCC 工具链:

```bash
sudo apt install gcc-aarch64-linux-gnu
```

### 内核配置

基于索尼的 defconfig，开启 UAC2:

```bash
cd ~/nwa100_src_v2/vendor/nxp-opensource/kernel_imx/

# 使用索尼原始 defconfig 作为基线
make ARCH=arm64 android_dmp1_defconfig

# 开启 UAC2
echo "CONFIG_USB_CONFIGFS_F_UAC2=y" >> .config
make ARCH=arm64 olddefconfig
```

配置变化:
```diff
- # CONFIG_USB_CONFIGFS_F_UAC2 is not set
+ CONFIG_USB_CONFIGFS_F_UAC2=y
```

保留索尼所有原始配置不变 (DNC, CXD3778GF, 音频 HAL 相关等)。

### 编译

```bash
# 编译完整内核 + DTBs + 所有模块
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) Image.gz dtbs modules

# 产物
# arch/arm64/boot/Image.gz          ← 新内核镜像 (需要刷入)
# drivers/usb/gadget/function/usb_f_uac2.ko  ← UAC2 模块
# drivers/usb/gadget/function/u_audio.ko     ← UAC2 音频层
# 以及其他所有 .ko 模块
```

### 刷机

```bash
# 进入 fastboot
adb reboot bootloader

# 刷入新内核 (保留 system/vendor 不变)
fastboot flash boot Image.gz
fastboot reboot

# 验证
adb shell "zcat /proc/config.gz | grep UAC2"
# 应输出: CONFIG_USB_CONFIGFS_F_UAC2=y
```

---

## Bridge Daemon

### 核心逻辑

```
UAC2 gadget (card#2, capture)          cxd3778gf (card#1, playback)
┌────────────────────┐                  ┌────────────────────┐
│  pcmC2D0c          │   pcm_read()     │  pcmC1D0p          │
│  (USB IN from PC)  │ ───────────────→ │  (hires playback)  │
│                    │   pcm_write()    │                    │
│                    │ ←─────────────── │                    │
└────────────────────┘                  └────────────────────┘
```

### 文件清单

```
daemon/
├── Android.mk            — Android NDK 编译脚本
├── uac2_bridge.c         — 主程序 (~340 行)
├── usb_dac_ctl.sh        — 控制脚本 (~230 行)
└── init.usb_dac.rc       — init 触发器
```

### Bridge 编译

```bash
# NDK 编译
export NDK=/path/to/android-ndk-r21e
$NDK/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk \
                APP_ABI=arm64-v8a APP_PLATFORM=android-28

# 或用交叉编译器直接编译
aarch64-linux-android-clang -static -o uac2_bridge uac2_bridge.c -ltinyalsa
```

---

## 内核模块部署

```bash
adb root && adb remount

# 将所有模块推送到 vendor 分区
cd ~/nwa100_src_v2/vendor/nxp-opensource/kernel_imx/
for mod in $(find . -name '*.ko'); do
    adb push "$mod" /vendor/lib/modules/
done

# 加载 UAC2 模块
adb shell insmod /vendor/lib/modules/u_audio.ko
adb shell insmod /vendor/lib/modules/usb_f_uac2.ko

# 验证
ls /config/usb_gadget/g1/functions/uac2*   # 新 uac2 函数模板
cat /proc/asound/cards                      # 新 card#2 出现
```

---

## USB Gadget 配置

### 当前 gadget 拓扑

```
/config/usb_gadget/g1/
├── idVendor = 0x054C (Sony)
├── idProduct = 0x0CF2 (ADB)
├── configs/b.1/
│   ├── f1 → ffs.adb
│   ├── MaxPower = 500
│   └── bmAttributes = 192
└── functions/
    ├── ffs.adb/
    └── ffs.mtp/
```

### 添加 UAC2 函数

```bash
mkdir /config/usb_gadget/g1/functions/uac2.gs0
echo 3 > /config/usb_gadget/g1/functions/uac2.gs0/p_chmask     # 立体声
echo 3 > /config/usb_gadget/g1/functions/uac2.gs0/c_chmask
echo 192000 > /config/usb_gadget/g1/functions/uac2.gs0/p_srate # Hi-Res
echo 192000 > /config/usb_gadget/g1/functions/uac2.gs0/c_srate
echo 24 > /config/usb_gadget/g1/functions/uac2.gs0/p_ssize     # 24 bit
echo 24 > /config/usb_gadget/g1/functions/uac2.gs0/c_ssize
ln -s /config/usb_gadget/g1/functions/uac2.gs0 \
      /config/usb_gadget/g1/configs/b.1/f2
```

---

## 控制脚本接口

| 命令 | 行为 |
|------|------|
| `usb_dac on` | 加载模块 → bind UAC2 → 启动 bridge daemon |
| `usb_dac off` | 杀 bridge → unbind UAC2 → 清理 function |
| `usb_dac status` | 显示完整诊断信息 |

| 触发方式 | 实现 |
|----------|------|
| ADB shell | `adb shell usb_dac on` |
| Android 属性 | `setprop persist.sys.usb.dac 1` → init 监听触发 |
| Quick Settings Tile | 可选 APK 快捷开关 |

---

## 部署与持久化

### 开机自动加载模块

`/vendor/etc/init/hw/init.icx1293.imx8mm.rc` 追加:
```
on boot
    insmod /vendor/lib/modules/u_audio.ko
    insmod /vendor/lib/modules/usb_f_uac2.ko
```

UAC2 函数默认不绑定 (不干扰 ADB/MTP), 仅在用户执行 `usb_dac on` 时激活。

### 持久化属性

```bash
setprop persist.sys.usb.dac 1    # 开机自动启用
setprop persist.sys.usb.dac 0    # 默认关闭
```

配合 `init.usb_dac.rc` 中定义的属性触发:
```
on property:persist.sys.usb.dac=1
    exec u:r:su:s0 -- /vendor/bin/usb_dac_ctl.sh on

on property:persist.sys.usb.dac=0
    exec u:r:su:s0 -- /vendor/bin/usb_dac_ctl.sh off
```

---

## 风险

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 内核编译失败 | 阻塞 | NXP BSP 4.14.78 是最成熟的版本之一 |
| cxd3778gf PCM 被占用 | bridge 无法 open | 暂停 audioserver; 或用低功耗 PCM (pcmC1D2p) |
| USB 枚举异常 | PC 不识别 | 减少 gadget function 数量 |
| 采样率不匹配 | 爆音/无声 | bridge 集成 libsamplerate 重采样 |
| DNC 模块冲突 | 降噪失灵 | DNC 与 UAC2 走不同路径, 无直接依赖 |

---

## 附录: 真机音频属性备忘

```
tinymix -D 1 重要 control:
  ctl9:  master volume       (0-120)    主音量
  ctl10: l balance volume                左声道平衡
  ctl11: r balance volume                右声道平衡
  ctl12: master gain                     Master 增益
  ctl13: playback mute                   播放静音
  ctl24: output device       headphone   输出设备选择
  ctl25: analog input device off         模拟输入
  ctl26: headphone amp       smaster-se  耳机放大器模式
  ctl34: standby              Off        待机
  ctl36: headphone detect mode interrupt 插拔检测模式
  ctl38: playback latency    normal      播放延迟模式

/proc/icx_audio_cxd3778gf_data/:
  dgt       — 数字增益表 (二进制)
  tct       — 调谐参数 (二进制)
  ovt       — 过零表
  tct_nh    — 降噪头戴式调谐
  tct_sg    — 标准调谐
  tct_snw510 — 降噪调谐

/proc/regmon/cxd3778gf/:
  target    — 寄存器地址 (写)
  value     — 寄存器值 (读)
```
