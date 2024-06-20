# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

AM_CFLAGS = \
        -Wall -Wextra -Wno-unused-parameter \
        -I$(top_srcdir)/include

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = libzbc.pc
pkginclude_HEADERS = ../include/libzbc/zbc.h

noinst_HEADERS = ../include/zbc_private.h

lib_LTLIBRARIES = libzbc.la

EXTRA_DIST = exports

CFILES = \
	zbc.c \
	zbc_utils.c \
	zbc_sg.c \
	zbc_scsi.c \
	zbc_ata.c

HFILES = \
	zbc.h \
	zbc_utils.h \
	zbc_sg.h

libzbc_la_DEPENDENCIES = exports
libzbc_la_SOURCES = $(CFILES) $(HFILES)
libzbc_la_CFLAGS = $(AM_CFLAGS) -fPIC
libzbc_la_LDFLAGS = \
        -Wl,--version-script,${srcdir}/exports \
	-version-number @LIBZBC_VERSION_LT@
