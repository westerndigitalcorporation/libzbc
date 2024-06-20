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

include report_domains/Makefile.am
include report_realms/Makefile.am
include zone_activate/Makefile.am
include dev_control/Makefile.am

if BUILD_GUI

polkitdir = $(datadir)/polkit-1/actions
polkit_action_in_FILES =

desktopdir = $(datadir)/applications
desktop_in_FILES =

pixmapdir = $(datadir)/pixmaps
pixmap_FILES =

bin_subst = sed -e 's,[@]bindir[@],$(bindir),g'

include gui/Makefile.am
include viewer/Makefile.am

polkit_action_FILES = $(polkit_action_in_FILES:.policy.in=.policy)
polkit_DATA = $(polkit_action_FILES)

desktop_FILES = $(desktop_in_FILES:.desktop.in=.desktop)
desktop_DATA = $(desktop_FILES)

pixmap_DATA = $(pixmap_FILES)

CLEANFILES = $(desktop_FILES) $(polkit_action_FILES)

EXTRA_DIST = \
	$(polkit_action_in_FILES) \
	$(desktop_in_FILES) \
	$(pixmap_FILES)

endif
