/*
 * Copyright 2018 NXP
 * Copyright 2019 Sony Video & Sound Products Inc.
 * Copyright 2019 Sony Home Entertainment & Sound Products Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef IMX8MM_EVK_ANDROID_H
#define IMX8MM_EVK_ANDROID_H

#define CONFIG_BCB_SUPPORT
#define CONFIG_CMD_READ

#define CONFIG_ANDROID_AB_SUPPORT
#define CONFIG_AVB_SUPPORT
#define CONFIG_SUPPORT_EMMC_RPMB
#define CONFIG_SYSTEM_RAMDISK_SUPPORT
#define CONFIG_AVB_FUSE_BANK_SIZEW 0
#define CONFIG_AVB_FUSE_BANK_START 0
#define CONFIG_AVB_FUSE_BANK_END 0
#define CONFIG_FASTBOOT_LOCK
#define FSL_FASTBOOT_FB_DEV "mmc"

#ifdef CONFIG_SYS_MALLOC_LEN
#undef CONFIG_SYS_MALLOC_LEN
#define CONFIG_SYS_MALLOC_LEN           (96 * SZ_1M)
#endif

#define CONFIG_USB_FUNCTION_FASTBOOT
#define CONFIG_CMD_FASTBOOT
#define CONFIG_ANDROID_BOOT_IMAGE
#define CONFIG_FASTBOOT_FLASH
#define CONFIG_FASTBOOT_STORAGE_MMC

#define CONFIG_FSL_FASTBOOT
#define CONFIG_ANDROID_RECOVERY

#define CONFIG_FASTBOOT_BUF_ADDR   CONFIG_SYS_LOAD_ADDR
#define CONFIG_FASTBOOT_BUF_SIZE   0x19000000

#define CONFIG_CMD_BOOTA
#define CONFIG_SUPPORT_RAW_INITRD
#define CONFIG_SERIAL_TAG

#undef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_BOOTCOMMAND

#if defined(TARGET_BUILD_VARIANT_USER) && defined(CONFIG_SILENT_CONSOLE)
#define ENV_SILENT "silent=1\0"
#else
#define ENV_SILENT
#endif

#if defined(TARGET_BUILD_VARIANT_USER) && defined(CONFIG_PWD)
#define ENV_PWD "pwd=1\0"
#else
#define ENV_PWD
#endif

#define CONFIG_EXTRA_ENV_SETTINGS		\
	ENV_SILENT                              \
	ENV_PWD                                 \
	"splashpos=m,m\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"initrd_high=0xffffffffffffffff\0"	\
	"panel=HX83102D_LCD\0"			\
	"boot_logo=icx_uboot.bmp.gz\0"		\
	"kernel_logo=icx_kernel_logo.bmp.gz\0" \
	"unlocked_logo=boot_orange.bmp.gz\0"

/* Enable mcu firmware flash */
#ifdef CONFIG_FLASH_MCUFIRMWARE_SUPPORT
#define ANDROID_MCU_FRIMWARE_DEV_TYPE DEV_MMC
#define ANDROID_MCU_FIRMWARE_START 0x500000
#define ANDROID_MCU_FIRMWARE_SIZE  0x40000
#define ANDROID_MCU_FIRMWARE_HEADER_STACK 0x20020000
#endif

#define AVB_AB_I_UNDERSTAND_LIBAVB_AB_IS_DEPRECATED

#ifdef CONFIG_IMX_TRUSTY_OS
#define AVB_RPMB
#define KEYSLOT_HWPARTITION_ID 2
#define KEYSLOT_BLKS             0x1FFF
#define NS_ARCH_ARM64 1

#else /* CONFIG_IMX_TRUSTY_OS */
/* imx8m won't touch CAAM in non-secure world. */
#undef CONFIG_FSL_CAAM_KB
#endif /* CONFIG_IMX_TRUSTY_OS */

#ifdef CONFIG_SPL_BUILD
#undef CONFIG_BLK
#endif

#ifdef CONFIG_DUAL_BOOTLOADER
#define BOOTLOADER_RBIDX_OFFSET  0x3FE000
#define BOOTLOADER_RBIDX_START   0x3FF000
#define BOOTLOADER_RBIDX_LEN     0x08
#define BOOTLOADER_RBIDX_INITVAL 0
#endif

#ifdef CONFIG_IMX8M_4G_LPDDR4
#undef PHYS_SDRAM_SIZE
#define PHYS_SDRAM_SIZE          0xC0000000 /* 3GB */
#define PHYS_SDRAM_2             0x100000000
#define PHYS_SDRAM_2_SIZE        0x40000000 /* 1GB */
#undef CONFIG_NR_DRAM_BANKS
#define CONFIG_NR_DRAM_BANKS 2
#endif

#endif /* IMX8MM_EVK_ANDROID_H */
