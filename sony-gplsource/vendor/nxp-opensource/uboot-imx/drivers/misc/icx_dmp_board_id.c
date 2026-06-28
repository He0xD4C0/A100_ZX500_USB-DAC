/*
 * SVS ICX DMP board ID reader driver
 *
 * Copyright 2018 Sony Video & Sound Products Inc.
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
 */

#include <inttypes.h>
#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <misc.h>
#include <asm-generic/gpio.h>
#include <asm/mach-imx/gpio.h>
#include <icx_dmp_board_id.h>

/* Introduce gd pointer. */
DECLARE_GLOBAL_DATA_PTR;

struct icx_dmp_board_id icx_dmp_board_id = {
	ICX_DMP_SETID_UNKNOWN,
	ICX_DMP_BID_UNKNOWN,
};

/* (Virtual) icx dmp board id device name in device tree. */
const char icx_dmp_board_id_device_name[] = "icx_dmp_board_id";

void initr_icx_dmp_board_id(void)
{
	int		ret;
	struct udevice	*dev = NULL;

	/* Probe Virtual MISC icx_dmp_board_id (icx_dmp_board_id) device. */
	ret = uclass_get_device_by_name(UCLASS_MISC,
		icx_dmp_board_id_device_name, &dev
	);
	if ((ret != 0) || (dev == NULL))
	printf("%s: Can not probe key device. name=%s, ret=%d\n",
		__func__, icx_dmp_board_id_device_name, ret
	);
}

static int icx_dmp_bind(struct udevice *dev)
{
	return 0;
}

static int icx_dmp_probe(struct udevice *dev)
{
#if defined(CONFIG_TARGET_IMX8MM_DMP1)
	int		bid1 = gpio_get_value(IMX_GPIO_NR(4, 10));
	int		bid2 = gpio_get_value(IMX_GPIO_NR(4, 11));
	int		bid3 = gpio_get_value(IMX_GPIO_NR(4,  1));
	int		sid0 = gpio_get_value(IMX_GPIO_NR(1,  5));
	int		sid1 = gpio_get_value(IMX_GPIO_NR(1, 15));

	if (bid1 >= 0 && bid2 >= 0 && bid3 >= 0)
		icx_dmp_board_id.bid = (bid3 << 2) | (bid2 << 1) | bid1;
	if (sid0 >= 0)
		icx_dmp_board_id.sid0 = sid0;
	if (sid1 >= 0) {
		icx_dmp_board_id.sid1 = sid1;
		switch (sid1) {
		case ICX_DMP_SID1_ICX_1293:
			icx_dmp_board_id.setid = ICX_DMP_SETID_ICX_1293;
			break;
		case ICX_DMP_SID1_ICX_1295:
			icx_dmp_board_id.setid = ICX_DMP_SETID_ICX_1295;
			break;
		default:
			break;
		}
	}
	switch (icx_dmp_board_id.bid) {
	case ICX_DMP_BID_BB:
		/* Settings for BB */
		gpio_direction_input(IMX_GPIO_NR(2, 12)); /* SDCARD INS */
		gpio_direction_input(IMX_GPIO_NR(4, 5));  /* */
		gpio_direction_output(IMX_GPIO_NR(5, 3), 0); /* WR_LED_CTL*/
		pinctrl_select_state(dev, "BB");
		break;
	case ICX_DMP_BID_LF:
	case ICX_DMP_BID_ET0:
		/* Settings for LF/ET0 */
		gpio_direction_input(IMX_GPIO_NR(2, 12)); /* SDCARD INS */
		gpio_direction_input(IMX_GPIO_NR(4, 5));  /* */
		gpio_direction_output(IMX_GPIO_NR(5, 3), 0); /* WR_LED_CTL*/
		pinctrl_select_state(dev, "LF");
		break;
	default:
		break;
	}
#endif
	printf("%s: Board Info. "
		"bid=0x%lx, sid0=%d, sid1=%d, setid=%lu\n",
		__func__,
		icx_dmp_board_id.bid,
		icx_dmp_board_id.sid0,
		icx_dmp_board_id.sid1,
		icx_dmp_board_id.setid
	);
	return 0;
}

static const struct misc_ops icx_dmp_ops = {
	/* Operation not supported. */
};

static const struct udevice_id icx_dmp_ids[] = {
	{ .compatible = "svs,icx-dmp-board-id" },
	{}
};


U_BOOT_DRIVER(icx_dmp_board_id) = {
	.name =		"icx_dmp_board_id",
	.id =		UCLASS_MISC,
	.of_match =	icx_dmp_ids,
	.bind =		icx_dmp_bind,
	.probe =	icx_dmp_probe,
	.ops =		&icx_dmp_ops,
};
