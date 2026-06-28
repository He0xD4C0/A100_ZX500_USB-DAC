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

#if (!defined(ICX_DMP_BOARD_ID_H_INCLUDED))
#define ICX_DMP_BOARD_ID_H_INCLUDED

/* icx_dmp_board_id.setid values */
#define	ICX_DMP_SETID_UNKNOWN	(0)
#define	ICX_DMP_SETID_ICX_1293	(1293)
#define	ICX_DMP_SETID_ICX_1295	(1295)

/* icx_dmp_board_id.bid values */
#define	ICX_DMP_BID_BB		(0x0)
#define	ICX_DMP_BID_LF		(0x1)
#define	ICX_DMP_BID_ET0		(0x2)
#define	ICX_DMP_BID_ET		(0x3)
#define	ICX_DMP_BID_UNKNOWN	(~0UL)

/* icx_dmp_board_id.sid1 values
 * This value represents raw switch value.
 * See ICX_DMP_PIN_* macros.
 */
#define	ICX_DMP_SID1_ICX_1293	(0)
#define	ICX_DMP_SID1_ICX_1295	(1)

struct icx_dmp_board_id {
	unsigned long		setid;
	unsigned long	bid;	/*!< Board ID */
	int		sid0;	/*!< SID0 pin level. */
	int		sid1;	/*!< SID1 pin level. */
};

extern struct icx_dmp_board_id	icx_dmp_board_id;

void initr_icx_dmp_board_id(void);

#endif /* (!defined(ICX_DMP_BOARD_ID_H_INCLUDED)) */
