# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

SUBDIRS = . $(subdirs)

AM_CFLAGS = \
	-Wall -Wextra -Wno-unused-parameter \
	-I$(top_srcdir)/include

libzbc_ldadd = $(top_builddir)/lib/libzbc.la

dist_man8_MANS =

include info/Makefile.am
include report_zones/Makefile.am
include zone_op/Makefile.am
include reset_zone/Makefile.am
include open_zone/Makefile.am
include close_zone/Makefile.am
include finish_zone/Makefile.am
include read_zone/Makefile.am
include write_zone/Makefile.am

include set_write_ptr/Makefile.am
include set_zones/Makefile.am

if BUILD_GUI
include gui/Makefile.am
include viewer/Makefile.am
endif
