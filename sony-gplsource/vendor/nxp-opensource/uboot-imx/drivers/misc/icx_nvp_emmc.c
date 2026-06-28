/* nvp_emmc.c
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
#include <common.h>
#include <part.h>
#include <blk.h>

#include <linux/string.h>
#include <vsprintf.h>
#include <dm.h>
#include <misc.h>
#include <mmc.h>

#include <errno.h>
#include <nvp.h>


/***********/
/*@ Macros */
/***********/

/* for debug */
#define NVP_LOG_LEVEL	1	/* CRITICAL/ALWAYS/INFO/SPEW */

/* sector information */
#define NVP_TABLE_SECTOR            16
#define NVP_DATA_SECTOR_MIN         64
#define NVP_DATA_SECTOR_MAX         32767
#define NVP_DATA_SECTOR_SIZE_LARGE  16      /* Large sector: 8192 */
#define BLK_SIZE                    512

/* boot parameter area index */
enum {
	NVP_BOOT_PARAM_SYSINFO = 0,
	NVP_BOOT_PARAM_PASSWORD,
	NVP_BOOT_PARAM_BOOTMODE,
	NVP_BOOT_PARAM_PRINTK,
	NVP_BOOT_PARAM_HOLD,
	NVP_BOOT_PARAM_PWD,
	NVP_BOOT_PARAM_NUM,
};

/* system parameter area index */
enum {
	NVP_SYSTEM_PARAM_MID = 0,
	NVP_SYSTEM_PARAM_PCODE,
	NVP_SYSTEM_PARAM_SERIALNO,
	NVP_SYSTEM_PARAM_UFN,
	NVP_SYSTEM_PARAM_KAS,
	NVP_SYSTEM_PARAM_SHIP,
	NVP_SYSTEM_PARAM_TEST,
	NVP_SYSTEM_PARAM_GTY,
	NVP_SYSTEM_PARAM_CHECKER,
	NVP_SYSTEM_PARAM_SERIALNO2,
	NVP_SYSTEM_PARAM_NC_PARAM,
	NVP_SYSTEM_PARAM_BT_PSKEY,
	NVP_SYSTEM_PARAM_NVRAM_INIT,
	NVP_SYSTEM_PARAM_EXT_SHIP,
	NVP_SYSTEM_PARAM_BATT_CALIB,
	NVP_SYSTEM_PARAM_INS,
	NVP_SYSTEM_PARAM_NUM,
};

/* flag information */
#define NVP_FLAG_INS_INITIALIZED                0x494E5354

#define NVP_FLAG_BMD_DIAG                       0x67414964

#define NVP_FLAG_PRINTK				0x6B4E5270
#define NVP_FLAG_GTY				0x67747479

const char icx_nvp_init_device_name[] = "icxnvpinit";

/************/
/*@ Typedef */
/************/

/* parameter info */
typedef struct {
	void *param;		/* address to variable      */
	int  size;			/* data size [byte]         */
} nvp_param_info_t;

/* boot parameter typedef */
typedef struct {
	unsigned int    sysinfo;
	unsigned char   password[32];
	unsigned int    bootmode_flag;
	unsigned int    printk_flag;
	unsigned int    hold_mode;
	unsigned char   pwd_id[16];
} nvp_boot_parameter_t;

/* boot system parameter typedef */
typedef struct {
	unsigned int    modelid[16];
	unsigned char   pcode[5];
	unsigned char   serialno[16];
	unsigned char   ship[32];
	unsigned char   serialno2[64];
	unsigned int	gty_flag;
	unsigned int    ins_flag;
} nvp_system_parameter_t;


/********************/
/*@ Local variables */
/********************/

/* area offset table */
static unsigned int nvp_area_sector_table[NVP_ID_NUM] = {
	 0,							/* area 00 table : boot parameter		*/
	 1,							/* area 01 table : system parameter		*/
	 8,							/* area 08 table : boot image			*/
	 9,							/* area 09 table : hold image			*/
	10,							/* area 10 table : low battery image	*/
	13,							/* area 13 table : precharge image		*/
	14,							/* area 14 table : dead battery image	*/
};

/* boot parameter */
static nvp_boot_parameter_t nvp_boot_param;

static nvp_param_info_t nvp_boot_param_table[NVP_BOOT_PARAM_NUM] = {
/*   +-------------------------------------------- address to variable  */
/*   |                              +------------- size [byte]          */
/*   |                              |                                   */
	{&nvp_boot_param.sysinfo,      4},			/* System information   */
	{&nvp_boot_param.password[0], 32},			/* U-Boot password      */
	{&nvp_boot_param.bootmode_flag,4},          /* Boot mode flag       */
	{&nvp_boot_param.printk_flag,  4},          /* printk flag          */
	{&nvp_boot_param.hold_mode,    4},          /* Hold mode            */
	{&nvp_boot_param.pwd_id[0],   16},          /* PWD ID               */
};

/* system parameter */
static nvp_system_parameter_t nvp_system_param;

static nvp_param_info_t nvp_system_param_table[NVP_SYSTEM_PARAM_NUM] = {
/*   +---------------------------------------------- address to variable  */
/*   |                                +------------- size [byte]          */
/*   |                                |                                   */
	{&nvp_system_param.modelid[0],  64},          /* Model ID             */
	{&nvp_system_param.pcode[0],     5},          /* Product Code         */
	{&nvp_system_param.serialno[0], 16},          /* Serial Number        */
	{NULL,                           8},          /* FUP File Name        */
	{NULL,                          64},          /* K & S                */
	{&nvp_system_param.ship[0],     32},          /* Ship                 */
	{NULL,                           4},          /* Test mode flag       */
	{&nvp_system_param.gty_flag,     4},          /* Getty mode flag      */
	{NULL,                          16},          /* Checker flag         */
	{&nvp_system_param.serialno2[0],64},          /* Serial Number2       */
	{NULL,                          64},          /* NC driver parameter  */
	{NULL,                         512},          /* Bluetooth pskey      */
	{NULL,                           4},          /* NVRAM init flag      */
	{NULL,                           4},          /* Extended ship info   */
	{NULL,                           4},          /* Battery calibration  */
	{&nvp_system_param.ins_flag,     4},          /* Install flag         */
};

static int nvp_initialized = 0;


/***************/
/*@ Prototypes */
/***************/

static int nvp_get_param(char *dst, int size, int sector);

static int nvp_get_dev(void);
static int nvp_read(unsigned long src, void *dst, unsigned long size);


/********************/
/*@ Global routines */
/********************/

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_init
 * Abstruct : Initialize NVP driver
 * Input    : none
 * Returen  : 0
 *----------------------------------------------------------------------------*/
int nvp_emmc_init(void)
{
	char buf[BLK_SIZE];
	unsigned int *sector_buf;
	unsigned short *sector_buf_large;
	int sector;
	int i;
	int ret = 0;

	if (nvp_initialized)
		return -EALREADY;

	/* get NVP partition information */
	ret = nvp_get_dev();
	if (ret < 0) {
		return ret;
	}

	/*******************************************/
	/* get boot parameters (Small sector type) */
	/*******************************************/
	memset(&nvp_boot_param, 0, sizeof(nvp_boot_param));
	nvp_boot_param.sysinfo = NVP_SYSINFO_INVALID;

	sector = NVP_TABLE_SECTOR + nvp_area_sector_table[NVP_ID_BOOT_PARAM];
	ret = nvp_read(sector, (void *)buf, BLK_SIZE);
	if (ret < 0) {
		return ret;
	}

	sector_buf = (unsigned int *)buf;

	for (i = 0; i < NVP_BOOT_PARAM_NUM; i++) {
		if ((NVP_DATA_SECTOR_MIN <= sector_buf[i]) && (sector_buf[i] <= NVP_DATA_SECTOR_MAX)) {
			if (nvp_boot_param_table[i].param == NULL)
				continue;

			nvp_get_param(nvp_boot_param_table[i].param, nvp_boot_param_table[i].size, sector_buf[i]);

			printf("[NVP] boot param %d: %08X\n",
					i, *(unsigned int *)nvp_boot_param_table[i].param);
		}
	}

	/********************************************/
	/* get system parameters (Large sector type */
	/********************************************/
	memset(&nvp_system_param, 0, sizeof(nvp_system_param));

	sector = NVP_TABLE_SECTOR + nvp_area_sector_table[NVP_ID_SYSTEM_PARAM];
	ret = nvp_read(sector, (void *)buf, BLK_SIZE);
	if (ret < 0) {
		return ret;
	}

	sector_buf_large = (unsigned short *)buf;

	for (i = 0; i < NVP_SYSTEM_PARAM_NUM; i++) {
		if ((NVP_DATA_SECTOR_MIN <= (sector_buf_large[i] * NVP_DATA_SECTOR_SIZE_LARGE)) &&
		    ((sector_buf_large[i] * NVP_DATA_SECTOR_SIZE_LARGE) <= NVP_DATA_SECTOR_MAX)) {
			if (nvp_system_param_table[i].param == NULL)
				continue;

			nvp_get_param(nvp_system_param_table[i].param, nvp_system_param_table[i].size, sector_buf_large[i] * NVP_DATA_SECTOR_SIZE_LARGE);

			printf("[NVP] system param %d: %02X %02X %02X %02X\n",
					i, *(unsigned char *)&nvp_system_param_table[i].param[0], *(unsigned char *)&nvp_system_param_table[i].param[1], *(unsigned char *)&nvp_system_param_table[i].param[2], *(unsigned char *)&nvp_system_param_table[i].param[3]);
		}
	}

	nvp_initialized = 1;

	return ret;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_bootmode
 * Abstruct : Get boot mode from bootmode(BMD) flag
 * Input    : none
 * Return   : NVP_BOOTMODE_NORMAL: normal(android) boot
 *          : NVP_BOOTMODE_INIT  : initialize mode
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_bootmode(void)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0)
			return NVP_BOOTMODE_NORMAL;
	}

	if (nvp_system_param.ins_flag != NVP_FLAG_INS_INITIALIZED)
		return NVP_BOOTMODE_INIT;

	if (nvp_boot_param.bootmode_flag == NVP_FLAG_BMD_DIAG)
		return NVP_BOOTMODE_DIAG;

	return NVP_BOOTMODE_NORMAL;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_check_printk
 * Abstruct : Check the printk eflag
 * Input    : none
 * Return   : 1:printk enable / 0:printk disable
 *----------------------------------------------------------------------------*/
int nvp_emmc_check_printk(void)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return 0;
		}
	}

	if (nvp_boot_param.printk_flag == NVP_FLAG_PRINTK) {
		return 1;
	}
	else {
		return 0;
	}
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_check_gty
 * Abstruct : Check the getty flag
 * Input    : none
 * Return   : 1:getty enable / 0:getty disable
 *----------------------------------------------------------------------------*/
int nvp_emmc_check_gty(void)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return 0;
		}
	}

	if (nvp_system_param.gty_flag == NVP_FLAG_GTY) {
		return 1;
	}
	else {
		return 0;
	}
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_modelid
 * Abstruct : Get model ID
 * Input    : p_modelid
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_modelid(unsigned int *p_modelid, size_t size)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	if (size > nvp_system_param_table[NVP_SYSTEM_PARAM_MID].size)
		return -1;

	memcpy(p_modelid, &nvp_system_param.modelid[0], size);
	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_serialno
 * Abstruct : Get serial number
 * Input    : serialno
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_serialno(char *serialno, size_t size)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	if (size > nvp_system_param_table[NVP_SYSTEM_PARAM_SERIALNO].size)
		return -1;

	memcpy(serialno, &nvp_system_param.serialno[0], size);
	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_pcode
 * Abstruct : Get product code (=F Number)
 * Input    : pcode
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_pcode(char *pcode, size_t size)
{
	int ret;
	unsigned int productcode = 0;
	unsigned int i;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	/*size should be 8 byte*/
	if(size < 8)
		return -1;

	for (i = 0; i < 5; i++) {
        productcode *= 10;
        productcode += ((nvp_system_param.pcode[i] >> 4) & 0x0f);
        productcode *= 10;
        productcode += (nvp_system_param.pcode[i] & 0x0f);
	}

	snprintf(pcode, 8, "%07X", productcode);
	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_ship
 * Abstruct : Get Ship Information
 * Input    : ship
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_ship(unsigned int *ship, size_t size)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	if (size > nvp_system_param_table[NVP_SYSTEM_PARAM_SHIP].size)
		return -1;

	memcpy(ship, &nvp_system_param.ship[0], size);
	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_serialno2
 * Abstruct : Get serial number2
 * Input    : serialno2
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_serialno2(char *serialno2, size_t size)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	if (size > nvp_system_param_table[NVP_SYSTEM_PARAM_SERIALNO2].size)
		return -1;

	memcpy(serialno2, &nvp_system_param.serialno2[0], size);
	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_sysinfo
 * Abstruct : Get System Informationr
 * Input    : sysinfo
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_sysinfo(unsigned int* p_sysinfo)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	*p_sysinfo = nvp_boot_param.sysinfo;
	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_emmc_get_pwd
 * Abstruct : Get a password
 * Input    : p_pwd : buffer address saved password
 *          : size  : length of p_pwd
 * Return   : 0:success -1:fail
 *----------------------------------------------------------------------------*/
int nvp_emmc_get_pwd(char *p_pwd, size_t size)
{
	int ret;

	if (nvp_initialized == 0) {
		ret = nvp_emmc_init();
		if (ret < 0) {
			return ret;
		}
	}

	memcpy(p_pwd, nvp_boot_param.password, size);

	return 0;
}

/*******************/
/*@ Local routines */
/*******************/

/*-----------------------------------------------------------------------------
 * Function : nvp_get_param
 * Abstruct : Get parameter
 * Input    : dst       : buffer
 *          : size      : size
 *          : sector    : NVP sector
 * Return   : 0:OK/-1:NG
 *----------------------------------------------------------------------------*/
static int nvp_get_param(char *dst, int size, int sector)
{
	char buf[BLK_SIZE];
	int  ret;
#if 0
	unsigned int i;
	unsigned char *p;
#endif

	ret = nvp_read(sector, (void *)buf, BLK_SIZE);
	if (ret < 0) {
		return ret;
	}

	/* save to information buffer */
	memcpy(dst, buf, size);
#if 0
	p = (unsigned char *)dst;
	for(i=0;i<(unsigned int)size;i++){
		if((i!=0)&&(i%16==0)) printf("\n");
		printf("%02X ", p[i]);
	}
	printf("\n");
#endif
	return 0;
}

/*==============*/
/* MMC/NAND I/F */
/*==============*/
static struct blk_desc *nvp_dev = NULL;
static disk_partition_t nvp_part = {0};

/*-----------------------------------------------------------------------------
 * Function : nvp_get_dev
 * Abstruct : Get MMC/NAND device information
 * Input    : none
 * Returen  : 0 : success
 *          : -ENODEV
 *----------------------------------------------------------------------------*/
static int nvp_get_dev(void)
{
	disk_partition_t info;
	int ret;
	int mmc_dev_no;

	mmc_dev_no = mmc_get_env_dev();

	nvp_dev = blk_get_dev("mmc",mmc_dev_no);
	if (!nvp_dev) {
		printf("[NVP] blk_get_dev, nvp_dev = NULL\n");
		return -ENODEV;
	}

	ret = part_get_info_by_name(nvp_dev, "nvp", &info);
	if (ret == -1) {
		printf("[NVP] part_get_info_by_name fail \n");
		return -ENODEV;
	}

	memcpy(&nvp_part, &info, sizeof(disk_partition_t));

	return 0;
}

/*-----------------------------------------------------------------------------
 * Function : nvp_read
 * Abstruct : Read a data from MMC/NAND
 * Input    : src     : source sector
 *          : dst     : destination address
 *          : size    : bytes
 * Returen  : read size
 *          : -EIO
 *----------------------------------------------------------------------------*/
static int nvp_read(unsigned long src, void *dst, unsigned long size)
{
	lbaint_t blk_start;
	lbaint_t blk_cnt;
	unsigned long len;
#if 0
	unsigned int i;
	unsigned char *p;
#endif

	blk_start = (nvp_part.start + (lbaint_t)src);
	blk_cnt = (lbaint_t)size / BLK_SIZE;

	len = blk_dread(nvp_dev, blk_start, blk_cnt, dst);
	if (len != blk_cnt) {
		printf("[NVP] failed to blk_dread , len = %08lx\n", len);
		return -EIO;
	}

#if 0
	p = (unsigned char *)dst;
	for(i=0;i<BLK_SIZE;i++){
		if((i!=0)&&(i%16==0)) printf("\n");
		printf("%02X ", p[i]);
	}
	printf("\n");
#endif

	return len;
}

/*==============*/
/* UBOOT DRIVER */
/*==============*/
static int icx_nvp_init_bind(struct udevice *dev)
{
	return 0;
}
static int icx_nvp_init_probe(struct udevice *dev)
{
	return 0;
}
static const struct misc_ops icx_nvp_init_ops = {
	/* Operation not supported. */
};
static const struct udevice_id icx_nvp_init_ids[] = {
	{ .compatible = "svs,icx-nvp-init" },
	{}
};
U_BOOT_DRIVER(icx_nvp_init) = {
	.name =		"icx_nvp_init",
	.id =		UCLASS_MISC,
	.of_match =	icx_nvp_init_ids,
	.bind =		icx_nvp_init_bind,
	.probe =	icx_nvp_init_probe,
	.ops =		&icx_nvp_init_ops,
};
