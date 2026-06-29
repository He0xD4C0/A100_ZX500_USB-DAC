# GPL 合规声明 — NW-A105 USB DAC 项目

## 一、原始 GPL 源码来源

本项目包含/修改以下 GPL/LGPL 许可的源码：

### 1.1 Sony Linux 内核源码

| 项目 | 详情 |
|------|------|
| **来源** | Sony Corporation — NW-A100 / NW-ZX500 系列 GPL 源码发布 |
| **下载地址** | https://oss.sony.net/Products/Linux/Audio/NW-A105_Ver20211130.html |
| **版本** | 20211130 |
| **内核版本** | Linux 4.14.78 (NXP i.MX8MM BSP) |
| **原厂配置文件** | `arch/arm64/configs/android_dmp1_defconfig` |
| **设备代号** | DMP1 (NW-A105 / ICX1293) |
| **存放路径** | `sony-gplsource/` (git submodule, commit `6ab67ba`) |
| **适用设备** | NW-A105, NW-A105HN, NW-A106, NW-A106HN, NW-A107, NW-A100TPS, NW-ZX505, NW-ZX507 |

### 1.2 tinyalsa

| 项目 | 详情 |
|------|------|
| **来源** | https://github.com/tinyalsa/tinyalsa |
| **许可证** | BSD 3-Clause (非 GPL) |
| **用途** | `daemon/uac2_bridge` 的 PCM ALSA 访问库 |
| **存放路径** | `daemon/tinyalsa/` (git submodule) |

---

## 二、对 Sony GPL 内核源码的修改

以下修改均在 `build-kernel.sh` 编译脚本中自动应用，**未改动 `sony-gplsource/` 子模块内已跟踪的文件**（保持 git submodule 干净）。

### 2.1 内核配置修改

**文件**: `kernel_patches/nwa105_kernel_config`

基于 Sony 原厂 `android_dmp1_defconfig`，通过 `make olddefconfig` 生成完整配置后，
新增/修改以下配置项以启用 USB DAC 功能：

```text
# ─── USB Gadget / UAC2 驱动 ─────────────────────────────
CONFIG_USB_GADGET=y                              # 启用 USB Gadget 框架
CONFIG_USB_F_UAC2=m                              # USB Audio Class 2 函数驱动（模块）
CONFIG_USB_CONFIGFS=y                            # ConfigFS 接口
CONFIG_USB_CONFIGFS_F_UAC2=y                     #   注册 f_uac2 到 configfs
CONFIG_USB_CONFIGFS_SERIAL=y                     #   Serial (保留 ADB)
CONFIG_USB_CONFIGFS_ACM=y                        #   ACM
CONFIG_USB_CONFIGFS_NCM=y                        #   NCM
CONFIG_USB_CONFIGFS_ECM=y                        #   ECM
CONFIG_USB_CONFIGFS_RNDIS=y                      #   RNDIS
CONFIG_USB_CONFIGFS_MASS_STORAGE=y               #   Mass Storage
CONFIG_USB_CONFIGFS_F_FS=y                       #   FunctionFS
CONFIG_USB_CONFIGFS_F_MTP=y                      #   MTP
CONFIG_USB_CONFIGFS_F_ACC=y                      #   Accessory
CONFIG_USB_CONFIGFS_F_AUDIO_SRC=y                #   Audio Source
CONFIG_USB_CONFIGFS_F_MIDI=y                     #   MIDI
CONFIG_USB_CONFIGFS_F_HID=y                      #   HID
CONFIG_USB_CONFIGFS_UEVENT=y                     #   Uevent
CONFIG_CONFIGFS_FS=y                             # ConfigFS 文件系统

# ─── CXD3778GF DAC 驱动 ────────────────────────────────
CONFIG_SND_SOC_IMX_CXD3778GF=y                   # i.MX ↔ CXD3778GF 平台驱动
CONFIG_SND_SOC_CXD3778GF=y                       # CXD3778GF Codec 驱动
```

> **注意**: 原厂 `android_dmp1_defconfig` 仅有约 500 行精简配置项。
> `nwa105_kernel_config` 包含 `olddefconfig` 展开后的完整 5260 行，其中
> 大部分是内核默认值和原厂 defconfig 隐式选中项。以上仅列出了**由本项目
> 主动修改**的关键配置。

### 2.2 内核源码补丁

以下补丁在 `build-kernel.sh` 运行时通过 `sed` 应用，用于解决
GCC 15 / glibc 2.40+ 工具链的兼容性问题：

#### 补丁 1: SELinux classmap.h — 新增地址族

**文件**: `security/selinux/include/classmap.h`
**原因**: GCC 15 自带的 glibc 新增了 `PF_XDP`(44) 和 `PF_MCTP`(45) 两个
地址族，内核 4.14 的 `secclass_map` 数组缺少对应条目导致 `#error` 编译失败。

```diff
--- a/security/selinux/include/classmap.h
+++ b/security/selinux/include/classmap.h
@@ -241,6 +241,10 @@ struct security_class_mapping secclass_map[] = {
        { "smc_socket",
          { COMMON_SOCK_PERMS, NULL } },
+       { "xdp_socket",
+         { COMMON_SOCK_PERMS, NULL } },
+       { "mctp_socket",
+         { COMMON_SOCK_PERMS, NULL } },
        { "infiniband_pkey",
          { "access", NULL } },
...
-#if PF_MAX > 44
+#if PF_MAX > 46
```

> **影响**: 仅影响编译时生成 SELinux 策略头文件的宿主工具
> (`scripts/selinux/genheaders` / `scripts/selinux/mdp`)。
> 对内核运行时行为**无任何影响**——Android 9 不使用 XDP/MCTP 协议族。

#### 补丁 2: dtc 工具 — 修复 yylloc 重复定义

**文件**: `scripts/dtc/dtc-lexer.lex.c`
**原因**: 新版 flex/bison 导致 `dtc-parser.tab.c` 和 `dtc-lexer.lex.c`
都定义了全局变量 `yylloc`，链接时产生 `multiple definition of 'yylloc'` 错误。

```diff
--- a/scripts/dtc/dtc-lexer.lex.c
+++ b/scripts/dtc/dtc-lexer.lex.c
@@ -631,1 +631,1 @@
-YYLTYPE yylloc;
+extern YYLTYPE yylloc;
```

> **影响**: 仅影响编译时生成设备树编译器 (`scripts/dtc/dtc`)。
> `yylloc` 保留在 `dtc-parser.tab.c` 中唯一定义。

### 2.3 未修改的部分

- **设备树 (DTS)**: `sony-imx8mm-dmp1.dts` 及其 include 文件完全来自 Sony，
  未做任何修改。
- **其他内核源码**: 除上述补丁外的所有文件均保持 Sony GPL 发布时的原始状态。
- **bootloader / ATF / Trusty**: 未使用本项目构建，直接使用 Sony 原厂镜像。

---

## 三、KernelSU 声明

### 3.1 本项目不包含 KernelSU 源码

本项目**没有将 KernelSU 代码纳入源码树或修改内核以集成 KernelSU**。

KernelSU 是一个独立项目，用于提供 Android 设备的 root 权限管理。
本项目仅在文档中要求设备**已安装** KernelSU (或 Magisk) 以提供 root 权限，
使 USB DAC App 能够通过 `su` 写入 `/config/usb_gadget/` 下的 configfs 接口。

### 3.2 KernelSU 官方来源

| 项目 | 地址 |
|------|------|
| **KernelSU** | https://github.com/tiann/KernelSU |
| **许可证** | GPL v2 (内核模块), GPL v3 (用户空间管理器) |

### 3.3 本项目中 KernelSU 相关的位置

KernelSU 在以下文件中仅作为**文档引用**出现，不是集成的代码：

| 文件 | 行 | 用途 |
|------|-----|------|
| `README.md:26` | `Root \| KernelSU 或 Magisk（必须）` | 硬件要求说明 |
| `README.md:116` | `Magisk / KernelSU 会弹出授权弹窗` | App 使用说明 |
| `PLAN.md:37` | `Root: KernelSU` | 技术方案说明 |

---

## 四、本项目其他独立组件的许可证

| 组件 | 路径 | 许可证 |
|------|------|--------|
| USB DAC Bridge | `daemon/uac2_bridge.c` | GPL v2 |
| Android App | `activity/` | MIT |
| 内核构建脚本 | `build-kernel.sh` | GPL v3 |
| Boot 打包脚本 | `build-bootimg.sh` | GPL v3 |
| 完整 GPL v3 文本 | `LICENCE` | GPL v3 |

---

## 五、源码获取

完整的 Corresponding Source（包括原始 Sony GPL 发布和本项目的所有修改）
可从本仓库获取：

> **https://github.com/He0xD4C0/A100_ZX500_USB-DAC**

- `sony-gplsource/` — Sony 原始 GPL 发布（未修改的子模块）
- `kernel_patches/nwa105_kernel_config` — 修改后的内核配置
- `build-kernel.sh` — 包含所有补丁的编译脚本（可审查、可复现）
- `daemon/uac2_bridge.c` — USB 音频桥接守护进程
- `activity/` — 控制 App 源码

---

*最后更新: 2026-06-29*
