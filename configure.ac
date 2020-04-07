# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

AC_INIT([libzbc], [5.8.5],
	[damien.lemoal@wdc.com, dmitry.fomichev@wdc.com],
	[libzbc], [https://github.com/hgst/libzbc])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([include/config.h])
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

AX_CHECK_ENABLE_DEBUG([no], [_DBG_])
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN"; then
	AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
fi

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_pattern_allow([AM_PROG_AR])
LT_INIT

ACX_PTHREAD
CFLAGS="$CFLAGS $EXTRA_CFLAGS $PTHREAD_CFLAGS"
if test "x$enable_debug" == "xno"; then
	CFLAGS="-O2 $CFLAGS"
fi
LIBS="$PTHREAD_LIBS $LIBS"
CC="$PTHREAD_CC"

# Checks for header files.
AC_CHECK_HEADER([scsi/scsi.h], [],
		[AC_MSG_ERROR([Couldn't find scsi/scsi.h])],
		[
			#include <scsi/scsi.h>
		])
AC_CHECK_HEADER([scsi/sg.h], [],
		[AC_MSG_ERROR([Couldn't find scsi/sg.h])],
		[
			#include <scsi/scsi.h>
			#include <scsi/sg.h>
		])
AC_CHECK_HEADER([libgen.h], [],
		[AC_MSG_ERROR([Couldn't find libgen.h])],
		[
			#include <libgen.h>
		])
AC_CHECK_HEADERS([linux/fs.h], [],
		[AC_MSG_ERROR([Couldn't find libgen.h])],
		[
			#include <linux/fs.h>
		])
AC_CHECK_HEADERS([linux/blkzoned.h], [], [],
		[
			#include <linux/blkzoned.h>
		])

# Conditionals

AC_ARG_ENABLE([gui],
	AS_HELP_STRING([--disable-gui], [Disable build of GUI tools (gzbc and gzviewer) [default=no]]))
AS_IF([test "x$enable_gui" != "xno"],
[
	# Build gzbc only if GTK3 is installed and can be detected with pkg-config.
	m4_ifdef([PKG_CHECK_MODULES],
		 [PKG_CHECK_MODULES([GTK], [gtk+-3.0], [HAVE_GTK3=1], [HAVE_GTK3=0])],
		 [HAVE_GTK3=0])
	AM_CONDITIONAL([BUILD_GUI], [test "$HAVE_GTK3" -eq 1])
],
[
	AM_CONDITIONAL([BUILD_GUI], false)
])

# Build test suite
AC_ARG_WITH([test],
            [AS_HELP_STRING([--with-test], [Build compatibility test suite [default=no]])],
            [WITH_TEST=1],
            [WITH_TEST=0])
AS_IF([test "$WITH_TEST" -eq 1],
[
	AM_CONDITIONAL([BUILD_TEST], true)
	AC_DEFINE([HAVE_DEVTEST], [1], ["Enable API test mode"])
],
[
	AM_CONDITIONAL([BUILD_TEST], false)
])

AC_CONFIG_FILES([
	libzbc.pc
	Makefile
])

AC_OUTPUT

