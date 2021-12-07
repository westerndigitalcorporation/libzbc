# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

SUBDIRS = . $(subdirs)

AM_CFLAGS = \
	-O2 \
	-Wall -Wextra -Wno-unused-parameter \
	-I$(top_srcdir)/include

libzbc_ldadd = $(top_builddir)/lib/libzbc.la

noinst_PROGRAMS =

if BUILD_TEST
include print_devinfo/Makefile.am
include report_zones/Makefile.am
include reset_zone/Makefile.am
include open_zone/Makefile.am
include close_zone/Makefile.am
include finish_zone/Makefile.am
include read_zone/Makefile.am
include write_zone/Makefile.am
endif
