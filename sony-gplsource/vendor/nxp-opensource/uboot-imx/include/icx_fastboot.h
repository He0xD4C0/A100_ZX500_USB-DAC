/* SPDX-License-Identifier:	GPL-2.0+ */
/*
 * Copyright 2018 Sony Video & Sound Products Inc.
 */

#if (!defined(ICX_FASTBOOT_INCLUDED))
#define ICX_FASTBOOT_INCLUDED

int icx_key_pressing_probe(void);
int icx_fastboot_force_unlock(void);
int is_fastboot_key_pressing(void);
int is_recovery_service_mode(void);

#endif /* (!defined(ICX_FASTBOOT_INCLUDED)) */
