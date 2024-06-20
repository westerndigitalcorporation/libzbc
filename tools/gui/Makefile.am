# SPDX-License-Identifier: LGPL-3.0-or-later
# 
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

polkit_action_in_FILES += gui/org.gnome.gzbc.policy.in
desktop_in_FILES += gui/gzbc.desktop.in
pixmap_FILES += gui/gzbc.png

bin_PROGRAMS += gzbc

gzbc_SOURCES = \
	gui/gzbc.c \
	gui/gzbc_if.c \
	gui/gzbc_if_dev.c \
	gui/gzbc.h

gzbc_CFLAGS = $(AM_CFLAGS) $(GTK_CFLAGS)
gzbc_LDADD = $(libzbc_ldadd) $(GTK_LIBS) -lpthread

dist_man8_MANS += gui/gzbc.8

gui/org.gnome.gzbc.policy: gui/org.gnome.gzbc.policy.in Makefile
	$(bin_subst) < gui/org.gnome.gzbc.policy.in > gui/org.gnome.gzbc.policy

gui/gzbc.desktop: gui/gzbc.desktop.in Makefile
	$(bin_subst) < gui/gzbc.desktop.in > gui/gzbc.desktop
