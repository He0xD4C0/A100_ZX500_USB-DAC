/*
 * nvp.h
 *
 * Copyright 2016-2018 Sony Corporation.
 * Copyright 2019 Sony Video & Sound Products Inc.
 * Copyright 2019 Sony Home Entertainment & Sound Products Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __NVP_H__
#define __NVP_H__

/************/
/* NVP area */
/************/

enum {
	NVP_ID_BOOT_PARAM = 0,
	NVP_ID_SYSTEM_PARAM,
	NVP_ID_BOOT_IMAGE,
	NVP_ID_HOLD_IMAGE,
	NVP_ID_LOWBATT_IMAGE,
	NVP_ID_PRECHARGE_IMAGE,
	NVP_ID_DEADBATT_IMAGE,
	NVP_ID_NUM
};


/*******************/
/* boot parameters */
/*******************/

/* system info */
#define NVP_SYSINFO_INVALID		0xFFFFFFFF

/* boot mode */
#define NVP_BOOTMODE_NORMAL  0
#define NVP_BOOTMODE_INIT    1
#define NVP_BOOTMODE_DIAG    2

extern const char icx_nvp_init_device_name[];
int nvp_emmc_init(void);
int nvp_emmc_get_bootmode(void);
int nvp_emmc_check_printk(void);
int nvp_emmc_check_gty(void);
int nvp_emmc_get_modelid(unsigned int *p_modelid, size_t size);
int nvp_emmc_get_serialno(char *serial, size_t size);
int nvp_emmc_get_pcode(char *pcode, size_t size);
int nvp_emmc_get_ship(unsigned int *p_ship, size_t size);
int nvp_emmc_get_sysinfo(unsigned int *p_sysinfo);
int nvp_emmc_get_serialno2(char *serial, size_t size);
int nvp_emmc_get_pwd(char *p_pwd, size_t size);

#ifdef CONFIG_ICX_NVP_EMMC
# define icx_nvp_init(a)                 nvp_emmc_init(a)
# define icx_nvp_get_bootmode(a)         nvp_emmc_get_bootmode(a)
# define icx_nvp_check_printk(a)         nvp_emmc_check_printk(a)
# define icx_nvp_check_gty(a)            nvp_emmc_check_gty(a)
# define icx_nvp_get_modelid(a, b)       nvp_emmc_get_modelid(a, b)
# define icx_nvp_get_serialno(a, b)      nvp_emmc_get_serialno(a, b)
# define icx_nvp_get_pcode(a, b)         nvp_emmc_get_pcode(a, b)
# define icx_nvp_get_ship(a, b)          nvp_emmc_get_ship(a, b)
# define icx_nvp_get_sysinfo(a)          nvp_emmc_get_sysinfo(a)
# define icx_nvp_get_serialno2(a, b)     nvp_emmc_get_serialno2(a, b)
# define icx_nvp_get_pwd(a, b)           nvp_emmc_get_pwd(a, b)
#elif CONFIG_ICX_NVP_NAND
/*These func use needs nvp nand driver implement*/
# define icx_nvp_init(a)                 nvp_nand_init(a)
# define icx_nvp_get_bootmode(a)         nvp_nand_get_bootmode(a)
# define icx_nvp_check_printk(a)         nvp_nand_check_printk(a)
# define icx_nvp_check_gty(a)            nvp_nand_check_gty(a)
# define icx_nvp_get_modelid(a, b)       nvp_nand_get_modelid(a, b)
# define icx_nvp_get_serialno(a, b)      nvp_nand_get_serialno(a, b)
# define icx_nvp_get_pcode(a, b)         nvp_nand_get_pcode(a, b)
# define icx_nvp_get_ship(a, b)          nvp_nand_get_ship(a, b)
# define icx_nvp_get_sysinfo(a)          nvp_nand_get_sysinfo(a)
# define icx_nvp_get_serialno2(a, b)     nvp_nand_get_serialno2(a, b)
# define icx_nvp_check_fw_update(a)      nvp_nand_check_fw_update(a)
# define icx_nvp_get_pwd(a, b)           nvp_nand_get_pwd(a, b)
#endif

#endif

