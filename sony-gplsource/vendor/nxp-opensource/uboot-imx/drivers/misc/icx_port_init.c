/*
 * SVS ICX port initialize driver
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

#include <inttypes.h>
#include <common.h>
#include <errno.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <fdt.h>
#include <linux/libfdt.h>
#include <misc.h>
#include <linux/string.h>
#include <asm-generic/gpio.h>
#include <asm/arch-imx8m/gpio.h>
#include <icx_port_init.h>
#include <dt-bindings/gpio/icx_port_init_dt.h>

/* Introduce gd pointer. */
DECLARE_GLOBAL_DATA_PTR;

/* (Virtual) icx port init device name in device tree. */
const char icx_port_init_device_name[] = "icxportinit";
const char icx_port_init_pinctrl_default[] = "default";

struct prop_port_init_parser {
	const uint8_t	*prop_base;
	const uint8_t	*prop_cur;
	ssize_t		prop_size;
};

struct prop_port_init_one {
	uint32_t	gpio;
	uint32_t	level;
	const char	*label;
};


uint32_t prop_cell_to_u32(const uint8_t *pc)
{	uint32_t	a;

	a  = (uint32_t)(*(pc + 0)) << 24;
	a |= (uint32_t)(*(pc + 1)) << 16;
	a |= (uint32_t)(*(pc + 2)) <<  8;
	a |= (uint32_t)(*(pc + 3)) <<  0;
	return a;
}

ssize_t prop_str_pick(const uint8_t *pstr, ssize_t max)
{	ssize_t	len;
	len = strnlen((__force const char *)pstr, max);
	len++; /* skip NUL terminator */
	if (len > max) {
		printf("%s: Broken device tree property. pstr=%p, len=%ld, max=%ld\n",
			__func__, pstr, (long) len, (long) max
		);
		return len;
	}
	return len;
}

static void prop_port_init_parser_init(struct prop_port_init_parser *pp,
	const void *prop, ssize_t size)
{	pp->prop_base = prop;
	pp->prop_cur = prop;
	pp->prop_size = size;
}

static int prop_port_init_pick(struct prop_port_init_parser *pp,
	struct prop_port_init_one *pone)
{	ssize_t			pos;
	const uint8_t		*cur;
	ssize_t			remain;
	ssize_t			len;
	uint32_t		gpio;
	uint32_t		level;

	if ((IS_ERR_OR_NULL(pp->prop_base))
		|| (pp->prop_size <= 0))
		return -ENOENT;

	cur = pp->prop_cur;
	pos = cur - pp->prop_base;
	if (pos >= pp->prop_size)
		return -ENOENT;	/* No more port */

	gpio =  prop_cell_to_u32(cur + 0 * sizeof(uint32_t));
	level = prop_cell_to_u32(cur + 1 * sizeof(uint32_t));
	if ((!IMX_GPIO_IS_VALID(gpio)) || (level >= ICX_PORT_INIT_NUMS)) {
		printf("%s: Invalid GPIO or level. gpio=0x%.8lx, level=%d\n",
			__func__, (ulong)gpio, level
		);
		return -EINVAL;
	}
	pone->gpio =  gpio;
	pone->level = level;

	cur += 2 * sizeof(uint32_t);
	pos += 2 * sizeof(uint32_t);

	remain = pp->prop_size - pos;

	pone->label = (__force const char *)cur;
	len = prop_str_pick(cur, remain);
	if (len > remain) {
		/* Overrun, anyway we over cursor pointer beyond size. */
		cur += len;
		pp->prop_cur = cur;
		return -EINVAL;
	}
	cur += len;
	pp->prop_cur = cur;
	return 0;
}

static int prop_port_init_one_config(struct prop_port_init_one *pone)
{	int	ret;

	uint32_t	gpio;
	uint32_t	level;
	const char	*label;

	gpio =  pone->gpio;
	level = pone->level;
	label = pone->label;

	ret = gpio_request(gpio, label);
	if (ret) {
		printf("%s: Can not request port. gpio=0x%.8lx, label=%s\n",
			__func__, (ulong)gpio, label
		);
		return ret;
	}

	ret = 0;
	switch (level) {
	case ICX_PORT_INIT_LOW:
		ret = gpio_direction_output(gpio, 0);
		break;
	case ICX_PORT_INIT_HIGH:
		ret = gpio_direction_output(gpio, 1);
		break;
	case ICX_PORT_INIT_IN:
		ret = gpio_direction_input(gpio);
		break;
	default:
		printf("%s: May broken device-tree property. gpio=0x%.8lx, level=0x%.8lx, label=%s\n",
			__func__,
			(ulong)gpio,
			(ulong)level,
			pone->label
		);
		return -EINVAL;
	}
	if (ret != 0) {
		printf("%s: Can not configure GPIO. gpio=0x%.8lx, level=0x%.8lx, label=%s\n",
			__func__,
			(ulong)gpio,
			(ulong)level,
			pone->label
		);
		return -EIO;
	}
	return ret;
}

#define PORT_NUM_MAX	(5 * 32)	/* GPIO1..GPIO5 */

static int icx_port_init_probe_config_gpio(struct udevice *dev)
{	struct prop_port_init_parser	pp;
	struct prop_port_init_one	pone;
	const char			*port_init = "svs,port-init";
	const struct fdt_property	*fdt_prop;
	int				prop_len = 0;
	const void			*fdt = gd->fdt_blob;
	int				node = dev_of_offset(dev);
	int				ret = 0;
	int				ret_config;
	int				count;


	fdt_prop = fdt_get_property(fdt, node, port_init, &prop_len);
	if (IS_ERR_OR_NULL(fdt_prop)) {
		printf("%s: Can not find property. name=%s\n",
			__func__, port_init
		);
		return -ENOENT;
	}
	printf("%s: port initialize list. base=0x%p, length=%d\n",
		__func__, fdt_prop->data, prop_len
	);
	prop_port_init_parser_init(&pp, fdt_prop->data, prop_len);
	count = 0;
	while (prop_port_init_pick(&pp, &pone) == 0) {
		ret_config = prop_port_init_one_config(&pone);
		if (ret_config)
			ret = ret ? ret : ret_config;

		/* Anyway continue loop. */
		count++;
		if (count >= PORT_NUM_MAX) {
			printf("%s: Too long port initialize list. name=%s\n",
				__func__, port_init
			);
			return -ENOENT;
		}
	}
	return ret;
}

void initr_icx_port_init(void)
{	int		ret;
	struct udevice	*dev = NULL;

	/* Probe Virtual MISC icx_port_init (icxportinit) device. */
	ret = uclass_get_device_by_name(UCLASS_MISC,
		icx_port_init_device_name, &dev
	);
	if ((ret != 0) || (dev == NULL))
		printf("%s: Can not probe port init device. name=%s, ret=%d\n",
		__func__, icx_port_init_device_name, ret
		);
}

static int icx_port_init_bind(struct udevice *dev)
{	return 0;
}

static int icx_port_init_probe(struct udevice *dev)
{	pinctrl_select_state(dev, icx_port_init_pinctrl_default);
	(void) icx_port_init_probe_config_gpio(dev);
	return 0;
}

static const struct misc_ops icx_port_init_ops = {
	/* Operation not supported. */
};

static const struct udevice_id icx_port_init_ids[] = {
	{ .compatible = "svs,icx-port-init" },
	{}
};


U_BOOT_DRIVER(icx_port_init) = {
	.name =		"icx_port_init",
	.id =		UCLASS_MISC,
	.of_match =	icx_port_init_ids,
	.bind =		icx_port_init_bind,
	.probe =	icx_port_init_probe,
	.ops =		&icx_port_init_ops,
};
