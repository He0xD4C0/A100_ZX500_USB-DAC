/*
 * sony_max1704x.c
 *
 * Copyright 2018 Sony Video & Sound Products Inc.
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
 */

#include <common.h>
#include <linux/err.h>
#include <dm.h>
#include <i2c.h>
#include <icx_dmp_board_id.h>
#include <power/sony_max1704x.h>

#define VERIFY_AND_FIX 1
#define LOAD_MODEL !(VERIFY_AND_FIX)

struct sony_max1704x_platform_data {
	int	vcell_reset;
	u16	rcomp_ini;
	int	temp_co_up;
	int	temp_co_down;
	u8	model_data[MAX1704X_MODEL_DATA_SIZE];
	u8	rcomp_data[MAX1704X_RCOMP_DATA_SIZE];
	u16	ocv_test;
	u16	check_min;
	u16	check_max;
};

DECLARE_GLOBAL_DATA_PTR;

static int max1704x_read_reg(struct udevice *dev, u8 addr, u16 *val)
{
	u16 tmp;
	int rv = dm_i2c_read(dev, addr, (u8*)&tmp, 2);

	if (rv < 0)
		return rv;
	if (val)
		*val = __be16_to_cpu(tmp);
	return 0;
}

static int max1704x_write_reg(struct udevice *dev, u8 addr, u16 val)
{
	val = __cpu_to_be16(val);
	return dm_i2c_write(dev, addr, (u8*)&val, 2);
}

static int max1704x_modify_reg(struct udevice *dev, u8 addr, u16 mask, u16 val)
{
	u16 tmp;
	int rv = max1704x_read_reg(dev, addr, &tmp);

	if (rv < 0)
		return rv;
	return max1704x_write_reg(dev, addr, (tmp & ~mask) | (val & mask));
}

static int max1704x_unlock_model_access(struct udevice *dev, u16 *ocv)
{
	u16 val;
	int rv = 0;
	int n;

	for (n = 0; n < MAX1704X_LOCK_TRY_COUNT; n++) {
		/* unlock model acess */
		rv = max1704x_write_reg(dev, MAX1704X_REG_LOCK,
			MAX1704X_LOCK_DISABLE);
		if (rv < 0)
			return rv;
		/* verify model access unlocked */
		rv = max1704x_read_reg(dev, MAX1704X_REG_OCV, &val);
		if (rv < 0)
			return rv;
		if (val == 0xFFFF)
			continue;
		if (ocv)
			*ocv = val;
		return 0;
	}
	printf("%s: can not unlock model access.\n", __func__);
	return -EIO;
}

static int max1704x_lock_model_access(struct udevice *dev)
{
	int rv;

	/* lock model access */
	rv = max1704x_write_reg(dev, MAX1704X_REG_LOCK, MAX1704X_LOCK_ENABLE);
	if (rv < 0)
		return rv;

	/* wait chip ready (150ms-600ms) */
	mdelay(160);
	return 0;
}

int max1704x_get_battery_voltage(struct udevice *dev, int *microvolts)
{
	u16 vcell;
	int rv;

	rv = max1704x_read_reg(dev, MAX1704X_REG_VCELL, &vcell);
	if (rv < 0)
		return rv;

	if (microvolts)
		*microvolts = (u64)vcell * MAX1704X_VCELL_RATE / 1000;
	return 0;
}

static int max1704x_load_model_data(struct udevice *dev)
{
	struct sony_max1704x_platform_data *priv = dev_get_priv(dev);
	u16 val = 0, ocv = 0;
	int rv = 0;
	int n;
	int load_or_verify = LOAD_MODEL;

#if defined(CONFIG_ICX_DMP_BOARD_ID)
	switch (icx_dmp_board_id.setid) {
	case ICX_DMP_SETID_ICX_1293:
	case ICX_DMP_SETID_ICX_1295:
		break;
	default:
		return 0;
	}
#endif /* defined(CONFIG_ICX_DMP_BOARD_ID) */

	/* read STATUS */
	rv = max1704x_read_reg(dev, MAX1704X_REG_STATUS, &val);
	if (rv < 0)
		return rv;
	if (val & MAX1704X_STATUS_RI)
		load_or_verify = LOAD_MODEL;
	else
		load_or_verify = VERIFY_AND_FIX;
	for (n = 0; n < MAX1704X_LOAD_TRY_COUNT; n++) {
		/* unlock model acess */
		rv = max1704x_unlock_model_access(dev, &ocv);
		if (rv < 0)
			return rv;
		if (load_or_verify == LOAD_MODEL) {
			/* load model data */
			rv = dm_i2c_write(dev, MAX1704X_REG_TABLE,
				priv->model_data, MAX1704X_MODEL_DATA_SIZE);
			if (rv < 0)
				return rv;
			/* load rcomp data */
			rv = dm_i2c_write(dev, MAX1704X_REG_RCOMP,
				priv->rcomp_data, MAX1704X_RCOMP_DATA_SIZE);
			if (rv < 0)
				return rv;
		}
		/* write OCV test value */
		rv = max1704x_write_reg(dev, MAX1704X_REG_OCV,
			priv->ocv_test);
		if (rv < 0)
			return rv;
		/* disable hibernate */
		rv = max1704x_write_reg(dev, MAX1704X_REG_HIBRT,
			MAX1704X_HIBRT_DISABLE);
		if (rv < 0)
			return rv;
		/* lock model access */
		rv = max1704x_lock_model_access(dev);
		if (rv < 0)
			return rv;
		/* read SOC and compare to expected result */
		rv = max1704x_read_reg(dev, MAX1704X_REG_SOC, &val);
		if (rv < 0)
			return rv;
		val >>= 8;
		if (val >= priv->check_min && val <= priv->check_max)
			break;
		load_or_verify = LOAD_MODEL;
	}
	if (n >= MAX1704X_LOAD_TRY_COUNT) {
		printf("%s: can not load model data.\n", __func__);
		return -EIO;
	}
	/* unlock model acess */
	rv = max1704x_write_reg(dev, MAX1704X_REG_LOCK,
		MAX1704X_LOCK_DISABLE);
	if (rv < 0)
		return rv;
	/* setup RCOMP */
	rv = max1704x_modify_reg(dev, MAX1704X_REG_CONFIG,
		MAX1704X_CONFIG_RCOMP | MAX1704X_CONFIG_SLEEP |
		MAX1704X_CONFIG_ALSC | MAX1704X_CONFIG_ALRT,
		(priv->rcomp_ini << 8) | MAX1704X_CONFIG_ALSC);
	if (rv < 0)
		return rv;
	/* setup OCV */
	rv = max1704x_write_reg(dev, MAX1704X_REG_OCV, ocv);
	if (rv < 0)
		return rv;
	/* restore hibernate */
	rv = max1704x_write_reg(dev, MAX1704X_REG_HIBRT,
		MAX1704X_HIBRT_DEFAULT);
	if (rv < 0)
		return rv;
	/* lock model access */
	rv = max1704x_lock_model_access(dev);
	if (rv < 0)
		return rv;
	/* clear alart, RI */
	rv = max1704x_modify_reg(dev, MAX1704X_REG_STATUS,
		MAX1704X_STATUS_ALART_MASK | MAX1704X_STATUS_RI, 0x0000);
	if (rv < 0)
		return rv;
	printf("%s: complete to load model data.\n", __func__);
	/* disable sleep mode */
	rv = max1704x_write_reg(dev, MAX1704X_REG_MODE, 0x0000);
	if (rv < 0)
		return rv;
	/* setup VRESET */
	u16 vreset_val = (u64)priv->vcell_reset * 1000 / MAX1704X_VCELL_RATE;
	rv = max1704x_modify_reg(dev, MAX1704X_REG_VRESET,
		MAX1704X_VRESET_MASK, vreset_val);
	if (rv < 0)
		return rv;
	return 0;
}

static int max1704x_prop_read_u32i(
	struct udevice *dev, const char *name, u32 *val, int index)
{
	const void *fdt = gd->fdt_blob;
	int node = dev_of_offset(dev);
	const struct fdt_property *fdt_prop;
	int len;

	fdt_prop = fdt_get_property(fdt, node, name, &len);
	if (fdt_prop == NULL)
		return len;
	if (len < 4 * (index + 1))
		return -1;
	if (val) {
		memcpy(val, fdt_prop->data + index * 4, 4);
		*val = __be32_to_cpu(*val);
	}
	return 0;
}

static int max1704x_prop_read_u8ai(
	struct udevice *dev, const char *name, u8 *val, int size, int index)
{
	const void *fdt = gd->fdt_blob;
	int node = dev_of_offset(dev);
	const struct fdt_property *fdt_prop;
	int len;

	fdt_prop = fdt_get_property(fdt, node, name, &len);
	if (fdt_prop == NULL)
		return len;
	if (len < size * (index + 1))
		return -1;
	if (val)
		memcpy(val, fdt_prop->data + index * size, size);
	return 0;
}

static int max1704x_parse_dt(struct udevice *dev)
{
	struct sony_max1704x_platform_data *priv = dev_get_priv(dev);
	u32 val = 0;
	int index = 0;

#if defined(CONFIG_ICX_DMP_BOARD_ID)
	switch (icx_dmp_board_id.setid) {
	case ICX_DMP_SETID_ICX_1293:
		index = 0;
		break;
	case ICX_DMP_SETID_ICX_1295:
		index = 1;
		break;
	default:
		return 0;
	}
#endif /* defined(CONFIG_ICX_DMP_BOARD_ID) */

	if (max1704x_prop_read_u32i(dev, "vcell_reset", &val, 0) == 0)
		priv->vcell_reset = val;
	max1704x_prop_read_u32i(dev, "empty_adjustment", &val, index);
	max1704x_prop_read_u32i(dev, "full_adjustment", &val, index);
	if (max1704x_prop_read_u32i(dev, "rcomp0", &val, index) == 0)
		priv->rcomp_ini = val;
	if (max1704x_prop_read_u32i(dev, "temp_co_up", &val, index) == 0)
		priv->temp_co_up = val;
	if (max1704x_prop_read_u32i(dev, "temp_co_down", &val, index) == 0)
		priv->temp_co_down = val;
	if (max1704x_prop_read_u32i(dev, "ocv_test", &val, index) == 0)
		priv->ocv_test = val;
	if (max1704x_prop_read_u32i(dev, "soc_check_a", &val, index) == 0)
		priv->check_min = val;
	if (max1704x_prop_read_u32i(dev, "soc_check_b", &val, index) == 0)
		priv->check_max = val;
	max1704x_prop_read_u32i(dev, "bits", &val, index);
	if (max1704x_prop_read_u32i(dev, "rcomp_seg", &val, index) == 0) {
		int i;
		for (i = 0; i < MAX1704X_RCOMP_DATA_SIZE; i += 2) {
			priv->rcomp_data[i] = val >> 8;
			priv->rcomp_data[i + 1] = val;
		}
	}
	max1704x_prop_read_u8ai(dev, "model_data", priv->model_data,
		MAX1704X_MODEL_DATA_SIZE, index);

#if defined(MAX1704X_DEBUG)
	printf("vcell_reset  = %d\n", priv->vcell_reset);
	printf("rcomp_ini    = %d\n", priv->rcomp_ini);
	printf("temp_co_up   = %d\n", priv->temp_co_up);
	printf("temp_co_down = %d\n", priv->temp_co_down);
	printf("ocv_test     = %d\n", priv->ocv_test);
	printf("check_min    = %d\n", priv->check_min);
	printf("check_max    = %d\n", priv->check_max);
	printf("rcomp_seg    = 0x%04X\n", priv->rcomp_data[0] << 8 | priv->rcomp_data[1]);
	printf("model_data:\n");
	int i;
	for (i = 0; i < MAX1704X_MODEL_DATA_SIZE; i += 8)
		printf("%04X  %02X %02X %02X %02X %02X %02X %02X %02X\n",
			i, priv->model_data[i], priv->model_data[i + 1],
			priv->model_data[i + 2], priv->model_data[i + 3],
			priv->model_data[i + 4], priv->model_data[i + 5],
			priv->model_data[i + 6], priv->model_data[i + 7]);
#endif /* defined(MAX1704X_DEBUG) */

	return 0;
}

struct udevice *initr_max1704x(void)
{
	const int i2c_bus = 0;
	const int max1704x_i2c_addr = 0x36;
	struct udevice *bus, *dev;
	int ret;
	u16 ver = 0;
	int voltage = 0;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, i2c_bus);
		return NULL;
	}
	ret = dm_i2c_probe(bus, max1704x_i2c_addr, 0, &dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, max1704x_i2c_addr, i2c_bus);
		return NULL;
	}

	max1704x_read_reg(dev, MAX1704X_REG_VERSION, &ver);
	printf("%s: version = %04X\n", __func__, ver);
	max1704x_parse_dt(dev);
	max1704x_load_model_data(dev);
	max1704x_get_battery_voltage(dev, &voltage);
	printf("%s: voltage = %d uV\n", __func__, voltage);
	return dev;
}

static int max1704x_probe(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id max1704x_ids[] = {
	{ .compatible = "svs,max1704x", },
	{ }
};

U_BOOT_DRIVER(max1704x) = {
	.name			= "max1704x",
	.id			= UCLASS_I2C_GENERIC,
	.of_match		= max1704x_ids,
	.probe			= max1704x_probe,
	.priv_auto_alloc_size	= sizeof(struct sony_max1704x_platform_data),
};
