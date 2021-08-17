# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

AC_INIT([libzbc], [5.12.0],
	[damien.lemoal@wdc.com, dmitry.fomichev@wdc.com],
	[libzbc], [https://github.com/westerndigitalcorporation/libzbc])

# Package version details: <major>.<minor>.<release>
PACKAGE_VERSION_MAJOR=$(echo $PACKAGE_VERSION | awk -F. '{print $1}')
PACKAGE_VERSION_MINOR=$(echo $PACKAGE_VERSION | awk -F. '{print $2}')
PACKAGE_VERSION_RELEASE=$(echo $PACKAGE_VERSION | awk -F. '{print $3}')

# libtool friendly library version format
LIBZBC_VERSION_LT=$PACKAGE_VERSION_MAJOR:$PACKAGE_VERSION_MINOR:$PACKAGE_VERSION_RELEASE
AC_SUBST([LIBZBC_VERSION_LT])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([include/config.h])
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

RPM_RELEASE=1
AC_SUBST(RPM_RELEASE)
AX_RPM_INIT

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
CFLAGS="$CFLAGS --std=gnu90 $EXTRA_CFLAGS $PTHREAD_CFLAGS"
if test "x$enable_debug" == "xno"; then
	CFLAGS="-O2 $CFLAGS"
fi
LIBS="$PTHREAD_LIBS $LIBS"
CC="$PTHREAD_CC"

# Checks for header files.
AC_CHECK_HEADERS([linux/fs.h], [],
		[AC_MSG_ERROR([Couldn't find linux/fs.h])],
		[[
			#ifdef HAVE_LINUX_FS_H
			#include <linux/fs.h>
			int main(int argc, char **argv) { return 0; }
			#endif
		]])
AC_CHECK_HEADERS([linux/blkzoned.h], [], [],
		[[
			#ifdef HAVE_LINUX_BLKZONED_H
			#include <linux/blkzoned.h>
			int main(int argc, char **argv) { return 0; }
			#endif
		]])

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

# Checks for rpm package builds
AC_PATH_PROG([RPMBUILD], [rpmbuild], [notfound])
AC_PATH_PROG([RPM], [rpm], [notfound])
AM_CONDITIONAL(ENABLE_RPMBUILD, test "x$RPMBUILD" != "xnotfound" && test "x$RPM" != "xnotfound")

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	lib/libzbc.pc
	tools/Makefile
	test/programs/Makefile
])

AC_OUTPUT

