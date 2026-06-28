#
# Copyright (C) 2014 The Android Open Source Project
# Copyright 2018 Sony Video & Sound Products Inc.
# Copyright 2019 Sony Home Entertainment & Sound Products Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

LOCAL_PATH := $(call my-dir)

#
# To update:
# @todo write procedure here.
#
#
# To add or remove applet(s) to/from busybox.
# @todo Write procedure here. \
#       Under develop.
#
# Configure with menuconfig.
# $ make menuconfig
# $ make
# $ ./gen-androidmk.sh

# BEGIN: source_files
busybox_SRC_FILES := \
	libbb/appletlib.c \
	libbb/compare_string_array.c \
	libbb/default_error_retval.c \
	libbb/get_last_path_component.c \
	libbb/last_char_is.c \
	libbb/messages.c \
	libbb/ptr_to_globals.c \
	libbb/xfunc_die.c \
	libbb/xfuncs.c \
	libbb/xfuncs_printf.c \
	libbb/copyfd.c \
	libbb/full_write.c \
	libbb/perror_msg.c \
	libbb/read.c \
	libbb/safe_write.c \
	libbb/signals.c \
	libbb/time.c \
	libbb/verror_msg.c \
	libbb/bb_strtonum.c \
	miscutils/adjtimex.c \
	miscutils/dc.c \
	miscutils/devmem.c \
	miscutils/hexedit.c \
	miscutils/i2c_tools.c \
	miscutils/inotifyd.c \
	miscutils/less.c \
	miscutils/lsscsi.c \
	miscutils/makedevs.c \
	miscutils/microcom.c \
	miscutils/nandwrite.c \
	miscutils/partprobe.c \
	miscutils/raidautorun.c \
	miscutils/readahead.c \
	miscutils/rx.c \
	miscutils/setfattr.c \
	miscutils/setserial.c \
	miscutils/strings.c \
	miscutils/time.c \
	miscutils/ttysize.c \
	miscutils/ubi_tools.c \
	miscutils/ubirename.c \
	miscutils/volname.c \
	miscutils/watchdog.c \
	modutils/modinfo.c \
	modutils/modprobe-small.c \
	modutils/modutils.c \
	networking/arp.c \
	networking/arping.c \
	networking/brctl.c \
	networking/hostname.c \
	networking/ifconfig.c \
	networking/ifenslave.c \
	networking/ifupdown.c \
	networking/interface.c \
	networking/ip.c \
	networking/ipcalc.c \
	networking/nameif.c \
	networking/nbd-client.c \
	networking/nc.c \
	networking/netstat.c \
	networking/ping.c \
	networking/pscan.c \
	networking/route.c \
	networking/slattach.c \
	networking/ssl_client.c \
	networking/tls.c \
	networking/tls_aes.c \
	networking/tls_pstm.c \
	networking/tls_pstm_montgomery_reduce.c \
	networking/tls_pstm_mul_comba.c \
	networking/tls_pstm_sqr_comba.c \
	networking/tls_rsa.c \
	networking/traceroute.c \
	networking/tunctl.c \
	networking/vconfig.c \
	networking/wget.c \
	networking/libiproute/ip_parse_common_args.c \
	networking/libiproute/ipaddress.c \
	networking/libiproute/iplink.c \
	networking/libiproute/ipneigh.c \
	networking/libiproute/iproute.c \
	networking/libiproute/iprule.c \
	networking/libiproute/iptunnel.c \
	networking/libiproute/libnetlink.c \
	networking/libiproute/ll_addr.c \
	networking/libiproute/ll_map.c \
	networking/libiproute/ll_types.c \
	networking/libiproute/rt_names.c \
	networking/libiproute/rtm_map.c \
	networking/libiproute/utils.c \
	procps/free.c \
	procps/fuser.c \
	procps/iostat.c \
	procps/kill.c \
	procps/lsof.c \
	procps/mpstat.c \
	procps/nmeter.c \
	procps/pgrep.c \
	procps/pidof.c \
	procps/pmap.c \
	procps/ps.c \
	procps/pstree.c \
	procps/pwdx.c \
	procps/smemcap.c \
	procps/sysctl.c \
	procps/top.c \
	procps/uptime.c \
	procps/watch.c \
	runit/chpst.c \
	selinux/chcon.c \
	selinux/getsebool.c \
	selinux/matchpathcon.c \
	selinux/runcon.c \
	selinux/setsebool.c \
	shell/ash.c \
	shell/ash_ptr_hack.c \
	shell/cttyhack.c \
	shell/math.c \
	shell/random.c \
	shell/shell_common.c \
	util-linux/blkdiscard.c \
	util-linux/blkid.c \
	util-linux/blockdev.c \
	util-linux/cal.c \
	util-linux/chrt.c \
	util-linux/dmesg.c \
	util-linux/eject.c \
	util-linux/fallocate.c \
	util-linux/fatattr.c \
	util-linux/fdformat.c \
	util-linux/fdisk.c \
	util-linux/findfs.c \
	util-linux/flock.c \
	util-linux/freeramdisk.c \
	util-linux/fsfreeze.c \
	util-linux/fstrim.c \
	util-linux/getopt.c \
	util-linux/hexdump.c \
	util-linux/hexdump_xxd.c \
	util-linux/hwclock.c \
	util-linux/ionice.c \
	util-linux/ipcrm.c \
	util-linux/ipcs.c \
	util-linux/losetup.c \
	util-linux/lspci.c \
	util-linux/lsusb.c \
	util-linux/mesg.c \
	util-linux/mkfs_ext2.c \
	util-linux/mkfs_reiser.c \
	util-linux/mkfs_vfat.c \
	util-linux/more.c \
	util-linux/mountpoint.c \
	util-linux/nsenter.c \
	util-linux/rdev.c \
	util-linux/readprofile.c \
	util-linux/renice.c \
	util-linux/rev.c \
	util-linux/rtcwake.c \
	util-linux/script.c \
	util-linux/scriptreplay.c \
	util-linux/setarch.c \
	util-linux/setpriv.c \
	util-linux/setsid.c \
	util-linux/swaponoff.c \
	util-linux/switch_root.c \
	util-linux/taskset.c \
	util-linux/uevent.c \
	util-linux/umount.c \
	util-linux/unshare.c \
	util-linux/volume_id/get_devname.c \
	util-linux/volume_id/volume_id.c \
	util-linux/volume_id/xfs.c \
	util-linux/volume_id/bcache.c \
	util-linux/volume_id/btrfs.c \
	util-linux/volume_id/cramfs.c \
	util-linux/volume_id/exfat.c \
	util-linux/volume_id/ext.c \
	util-linux/volume_id/f2fs.c \
	util-linux/volume_id/fat.c \
	util-linux/volume_id/hfs.c \
	util-linux/volume_id/iso9660.c \
	util-linux/volume_id/jfs.c \
	util-linux/volume_id/linux_raid.c \
	util-linux/volume_id/linux_swap.c \
	util-linux/volume_id/luks.c \
	util-linux/volume_id/minix.c \
	util-linux/volume_id/nilfs.c \
	util-linux/volume_id/ntfs.c \
	util-linux/volume_id/ocfs2.c \
	util-linux/volume_id/reiserfs.c \
	util-linux/volume_id/romfs.c \
	util-linux/volume_id/squashfs.c \
	util-linux/volume_id/sysv.c \
	util-linux/volume_id/ubifs.c \
	util-linux/volume_id/udf.c \
	util-linux/volume_id/util.c \
	archival/bbunzip.c \
	archival/bzip2.c \
	archival/cpio.c \
	archival/gzip.c \
	archival/lzop.c \
	archival/tar.c \
	archival/unzip.c \
	archival/libarchive/common.c \
	archival/libarchive/data_extract_all.c \
	archival/libarchive/data_extract_to_command.c \
	archival/libarchive/data_extract_to_stdout.c \
	archival/libarchive/data_skip.c \
	archival/libarchive/decompress_bunzip2.c \
	archival/libarchive/decompress_gunzip.c \
	archival/libarchive/decompress_unlzma.c \
	archival/libarchive/decompress_unxz.c \
	archival/libarchive/filter_accept_list.c \
	archival/libarchive/filter_accept_reject_list.c \
	archival/libarchive/find_list_entry.c \
	archival/libarchive/get_header_cpio.c \
	archival/libarchive/get_header_tar.c \
	archival/libarchive/header_list.c \
	archival/libarchive/header_verbose_list.c \
	archival/libarchive/init_handle.c \
	archival/libarchive/lzo1x_1.c \
	archival/libarchive/lzo1x_1o.c \
	archival/libarchive/lzo1x_d.c \
	archival/libarchive/open_transformer.c \
	archival/libarchive/seek_by_jump.c \
	archival/libarchive/seek_by_read.c \
	archival/libarchive/unsafe_prefix.c \
	archival/libarchive/unsafe_symlink_target.c \
	archival/libarchive/data_align.c \
	archival/libarchive/filter_accept_all.c \
	archival/libarchive/header_skip.c \
	console-tools/clear.c \
	console-tools/reset.c \
	console-tools/resize.c \
	coreutils/basename.c \
	coreutils/cat.c \
	coreutils/chgrp.c \
	coreutils/chmod.c \
	coreutils/chown.c \
	coreutils/chroot.c \
	coreutils/cksum.c \
	coreutils/comm.c \
	coreutils/cp.c \
	coreutils/cut.c \
	coreutils/dd.c \
	coreutils/df.c \
	coreutils/dirname.c \
	coreutils/dos2unix.c \
	coreutils/du.c \
	coreutils/echo.c \
	coreutils/env.c \
	coreutils/expand.c \
	coreutils/expr.c \
	coreutils/factor.c \
	coreutils/false.c \
	coreutils/fold.c \
	coreutils/fsync.c \
	coreutils/head.c \
	coreutils/id.c \
	coreutils/link.c \
	coreutils/ln.c \
	coreutils/ls.c \
	coreutils/md5_sha1_sum.c \
	coreutils/mkdir.c \
	coreutils/mkfifo.c \
	coreutils/mknod.c \
	coreutils/mktemp.c \
	coreutils/mv.c \
	coreutils/nice.c \
	coreutils/nl.c \
	coreutils/nohup.c \
	coreutils/nproc.c \
	coreutils/od.c \
	coreutils/paste.c \
	coreutils/printenv.c \
	coreutils/printf.c \
	coreutils/pwd.c \
	coreutils/readlink.c \
	coreutils/realpath.c \
	coreutils/rm.c \
	coreutils/rmdir.c \
	coreutils/seq.c \
	coreutils/shred.c \
	coreutils/shuf.c \
	coreutils/sleep.c \
	coreutils/sort.c \
	coreutils/split.c \
	coreutils/stat.c \
	coreutils/stty.c \
	coreutils/sum.c \
	coreutils/tac.c \
	coreutils/tail.c \
	coreutils/tee.c \
	coreutils/test.c \
	coreutils/test_ptr_hack.c \
	coreutils/timeout.c \
	coreutils/touch.c \
	coreutils/tr.c \
	coreutils/true.c \
	coreutils/truncate.c \
	coreutils/uname.c \
	coreutils/uniq.c \
	coreutils/unlink.c \
	coreutils/usleep.c \
	coreutils/uudecode.c \
	coreutils/uuencode.c \
	coreutils/wc.c \
	coreutils/yes.c \
	coreutils/libcoreutils/cp_mv_stat.c \
	coreutils/libcoreutils/getopt_mk_fifo_nod.c \
	debianutils/pipe_progress.c \
	debianutils/run_parts.c \
	debianutils/which.c \
	klibc-utils/resume.c \
	e2fsprogs/chattr.c \
	e2fsprogs/e2fs_lib.c \
	e2fsprogs/fsck.c \
	e2fsprogs/lsattr.c \
	e2fsprogs/tune2fs.c \
	editors/awk.c \
	editors/cmp.c \
	editors/diff.c \
	editors/ed.c \
	editors/patch.c \
	editors/sed.c \
	editors/vi.c \
	findutils/find.c \
	findutils/xargs.c \
	init/halt.c \
	libbb/ask_confirmation.c \
	libbb/auto_string.c \
	libbb/bb_cat.c \
	libbb/bb_getgroups.c \
	libbb/bb_pwd.c \
	libbb/bb_qsort.c \
	libbb/capability.c \
	libbb/chomp.c \
	libbb/common_bufsiz.c \
	libbb/concat_path_file.c \
	libbb/concat_subpath_file.c \
	libbb/copy_file.c \
	libbb/crc32.c \
	libbb/device_open.c \
	libbb/dump.c \
	libbb/endofname.c \
	libbb/executable.c \
	libbb/fclose_nonstdin.c \
	libbb/fflush_stdout_and_exit.c \
	libbb/fgets_str.c \
	libbb/find_mount_point.c \
	libbb/find_pid_by_name.c \
	libbb/find_root_device.c \
	libbb/get_cpu_count.c \
	libbb/get_line_from_file.c \
	libbb/get_shell_name.c \
	libbb/get_volsize.c \
	libbb/getopt32.c \
	libbb/getopt_allopts.c \
	libbb/getpty.c \
	libbb/hash_md5_sha.c \
	libbb/herror_msg.c \
	libbb/human_readable.c \
	libbb/in_ether.c \
	libbb/inet_cksum.c \
	libbb/inet_common.c \
	libbb/inode_hash.c \
	libbb/isdirectory.c \
	libbb/isqrt.c \
	libbb/lineedit.c \
	libbb/lineedit_ptr_hack.c \
	libbb/llist.c \
	libbb/loop.c \
	libbb/make_directory.c \
	libbb/makedev.c \
	libbb/match_fstype.c \
	libbb/mode_string.c \
	libbb/parse_config.c \
	libbb/parse_mode.c \
	libbb/percent_decode.c \
	libbb/perror_nomsg_and_die.c \
	libbb/print_flags.c \
	libbb/print_numbered_lines.c \
	libbb/printable.c \
	libbb/printable_string.c \
	libbb/process_escape_sequence.c \
	libbb/procps.c \
	libbb/progress.c \
	libbb/read_key.c \
	libbb/read_printf.c \
	libbb/recursive_action.c \
	libbb/remove_file.c \
	libbb/replace.c \
	libbb/rtc.c \
	libbb/run_shell.c \
	libbb/safe_gethostname.c \
	libbb/safe_poll.c \
	libbb/safe_strncpy.c \
	libbb/selinux_common.c \
	libbb/single_argv.c \
	libbb/skip_whitespace.c \
	libbb/speed_table.c \
	libbb/str_tolower.c \
	libbb/strrstr.c \
	libbb/sysconf.c \
	libbb/trim.c \
	libbb/u_signal_names.c \
	libbb/ubi.c \
	libbb/udp_io.c \
	libbb/unicode.c \
	libbb/uuencode.c \
	libbb/vfork_daemon_rexec.c \
	libbb/warn_ignoring_args.c \
	libbb/wfopen.c \
	libbb/wfopen_input.c \
	libbb/write.c \
	libbb/xatonum.c \
	libbb/xconnect.c \
	libbb/xgetcwd.c \
	libbb/xgethostbyname.c \
	libbb/xreadlink.c \
	libbb/xrealloc_vector.c \
	libbb/xregcomp.c \
	libpwdgrp/uidgid_get.c \
	\
	libbb/bb_bswap_64.c \

# Keep avobe line intentionally blank.
# END: source_files

busybox_CFLAGS := \
	-std=gnu99 \
	-include vendor/sony/tools/systemtools/busybox/include/autoconf.h \
	-D_GNU_SOURCE \
	-DNDEBUG \
	-D_LARGEFILE_SOURCE \
	-D_LARGEFILE64_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-D"BB_VER=KBUILD_STR(1.28.3)" \
	-Wall \
	-Wshadow \
	-Wwrite-strings \
	-Wundef \
	-Wstrict-prototypes \
	-Wunused \
	-Wunused-parameter \
	-Wunused-function \
	-Wunused-value \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wno-format-security \
	-Wdeclaration-after-statement \
	-Wold-style-definition \
	-fno-builtin-strlen \
	-fomit-frame-pointer \
	-ffunction-sections \
	-fdata-sections \
	-funsigned-char \
	-static-libgcc \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables \
	-fno-builtin-printf \
	-Os \
	-D"KBUILD_STR(s)=\#s" \
	-D"KBUILD_BASENAME=KBUILD_STR(appletlib)" \
	-D"KBUILD_MODNAME=KBUILD_STR(appletlib)" \

# Keep avobe line intentionally blank.

# excluded
#	-fno-guess-branch-probability \
#	-finline-limit=0 \
#	-falign-functions=1 \
#	-falign-jumps=1 \
#	-falign-labels=1 \
#	-falign-loops=1 \

busybox_shared_libraries := liblog libsepol \
	libcutils libcrypto libz libm libpcre2 \
	libandroid_runtime \


busybox_static_libraries := libselinux \


busybox_LOCAL_C_INCLUDES := \
	vendor/sony/tools/systemtools/busybox/include \
	vendor/sony/tools/systemtools/libbb \
	bionic/libc/dns/include \
	external/openssh/openbsd-compat \
	external/openssh \
	external/selinux/libselinux/include \
	external/selinux/libsepol/include \

# Keep avobe line intentionally blank.

# not usable on Android?: freeramdisk fsfreeze install makedevs nbd-client
#                         partprobe pivot_root pwdx rev rfkill vconfig
# currently prefer BSD system/core/toolbox: dd
# currently prefer BSD external/netcat: nc netcat
# currently prefer external/e2fsprogs: chattr lsattr

ALL_TOOLS := \
	[ \
	[[ \
	adjtimex \
	arch \
	arp \
	ash \
	awk \
	bash \
	blkdiscard \
	brctl \
	chattr \
	chcon \
	chpst \
	cttyhack \
	dc \
	depmod \
	devmem \
	dnsdomainname \
	ed \
	eject \
	envuidgid \
	factor \
	fatattr \
	fdflush \
	fdformat \
	fdisk \
	findfs \
	fold \
	freeramdisk \
	fsfreeze \
	fstrim \
	fsync \
	fuser \
	getopt \
	getsebool \
	halt \
	hd \
	hexdump \
	hexedit \
	ifenslave \
	iostat \
	ipaddr \
	ipcalc \
	ipcrm \
	ipcs \
	iplink \
	ipneigh \
	iproute \
	iprule \
	iptunnel \
	killall5 \
	less \
	link \
	linux32 \
	linux64 \
	lsattr \
	lsscsi \
	lzcat \
	lzma \
	lzop \
	makedevs \
	matchpathcon \
	mesg \
	mkdosfs \
	mkfs.reiser \
	mkfs.vfat \
	mpstat \
	nameif \
	nanddump \
	nandwrite \
	nbd-client \
	nc \
	nmeter \
	nproc \
	nsenter \
	partprobe \
	pipe_progress \
	poweroff \
	pscan \
	pstree \
	pwdx \
	raidautorun \
	rdev \
	readahead \
	readprofile \
	reset \
	resize \
	resume \
	rev \
	route \
	rtcwake \
	run-parts \
	runcon \
	rx \
	script \
	scriptreplay \
	setarch \
	setfattr \
	setpriv \
	setsebool \
	setserial \
	setuidgid \
	sha3sum \
	shred \
	shuf \
	slattach \
	smemcap \
	softlimit \
	ssl_client \
	stty \
	sum \
	switch_root \
	test \
	traceroute \
	ttysize \
	tunctl \
	ubiattach \
	ubidetach \
	ubimkvol \
	ubirename \
	ubirmvol \
	ubirsvol \
	ubiupdatevol \
	uevent \
	unexpand \
	unlink \
	unlzma \
	unshare \
	unxz \
	unzip \
	vconfig \
	vi \
	volname \
	watch \
	watchdog \
	wget \
	xz \
	xzcat \

# Keep avobe line intentionally blank.

############################################
# busybox for /system
############################################

include $(CLEAR_VARS)
LOCAL_MODULE := busybox
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := blkid
LOCAL_SRC_FILES := $(busybox_SRC_FILES)
LOCAL_CFLAGS := $(busybox_CFLAGS)
LOCAL_C_INCLUDES += \
	$(busybox_LOCAL_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := $(busybox_shared_libraries)
LOCAL_STATIC_LIBRARIES := $(busybox_static_libraries)
LOCAL_CXX_STL := none
LOCAL_POST_INSTALL_CMD := $(hide) $(foreach t,$(ALL_TOOLS),ln -sf ${LOCAL_MODULE} $(TARGET_OUT)/bin/$(t);)

#Must overwrite blkid for exFAT
LOCAL_POST_INSTALL_CMD += cp -f $(TARGET_OUT)/bin/${LOCAL_MODULE} $(TARGET_OUT)/bin/blkid
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/LICENSE
include $(BUILD_EXECUTABLE)
