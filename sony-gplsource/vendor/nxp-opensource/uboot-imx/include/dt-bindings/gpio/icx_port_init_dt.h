/*
 * SVS ICX port initialize driver device tree part
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

#ifndef _DT_BINDINGS_ICX_PORT_INIT_DT_H
#define _DT_BINDINGS_ICX_PORT_INIT_DT_H

#define IMX_GPIO_NR(port, index)	((((port)-1)*32)+((index)&31))
#define	IMX_GPIO_GROUP(nr)		((nr) >> 0x5)
#define	IMX_GPIO_GROUP_MAX		(5)
#define	IMX_GPIO_NR_MAX		(IMX_GPIO_NR(IMX_GPIO_GROUP_MAX, 31))
#define	IMX_GPIO_IS_VALID(nr)	((nr) <= IMX_GPIO_NR_MAX)

#define	ICX_PORT_INIT_LOW	(0)
#define	ICX_PORT_INIT_HIGH	(1)
#define	ICX_PORT_INIT_IN	(2)
#define	ICX_PORT_INIT_NUMS	(3)

#endif /* _DT_BINDINGS_ICX_PORT_INIT_DT_H */
