/*
 * Copyright 2019 Sony Home Entertainment & Sound Products Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <fuse.h>
#include <linux/errno.h>

static int do_srkchk_p(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	u32 val;

	if (argc > 1){
		return CMD_RET_USAGE;
	}

	ret = fuse_read(6, 0, &val);
	if (ret)
		goto err;

	if(val == 0x3b9123b1){
		printf("is set production key\n");
		ret = CMD_RET_SUCCESS;
	}else {
		printf("is not set production key\n");
		ret = CMD_RET_FAILURE;
	}

err:
	return ret;
}

U_BOOT_CMD(
	srkchk_p, CONFIG_SYS_MAXARGS, 0, do_srkchk_p,
	"SRK Check for production sub-system",
	"srkchk_p\n"
	":this cmd is for checking written production key\n"
);
