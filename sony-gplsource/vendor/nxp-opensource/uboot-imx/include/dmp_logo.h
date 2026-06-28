/*
 * Copyright 2019 Sony Home Entertainment & Sound Products Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef _DMP_LOGO_H
#define _DMP_LOGO_H

#ifdef CONFIG_VIDEO_BMP_DMP_LOGO
/* display logo file (*.bmp.gz) */
int display_dmp_logo_file(const char* file_name);
/* display logo at offset of partition */
int display_dmp_logo_offset(const char* part_name, const ulong ofset,
	const ulong count);
#else
int display_dmp_logo_file(const char* file_name) { return 0; }
int display_dmp_logo_offset(const char* part_name, const ulong ofset,
	const ulong count){ return 0; }
#endif

#endif /* _DMP_LOGO_H */
