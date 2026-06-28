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

#if (!defined(ICX_PORT_INIT_H_INCLUDED))
#define ICX_PORT_INIT_H_INCLUDED

#define	MISC_CLASS_ICX_PORT_INIT	(32)

/* DMP BB key ports.
 * Press key, then level goes Low.
 */
#define	ICX_DMP_KEY_PLAY      (IMX_GPIO_NR(3,  7)) /* GPIO3_IO7  */
#define	ICX_DMP_KEY_VOL_PLUS  (IMX_GPIO_NR(3, 16)) /* GPIO3_IO16 */
#define	ICX_DMP_KEY_VOL_MINUS (IMX_GPIO_NR(4,  0)) /* GPIO4_IO0  */
#define	ICX_DMP_KEY_FF	 (IMX_GPIO_NR(4, 28))	   /* GPIO4_IO28 */
#define	ICX_DMP_KEY_FR	 (IMX_GPIO_NR(5,  4))	   /* GPIO4_IO4 */
#define	ICX_DMP_KEY_HOLD (IMX_GPIO_NR(5,  8))	   /* GPIO5_IO8 */

#define	ICX_DMP_KEY_PRESSED	(0)
#define	ICX_DMP_KEY_RELEASED	(1)

void initr_icx_port_init(void);

#endif /* (!defined(ICX_PORT_INIT_H_INCLUDED)) */
