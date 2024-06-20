# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

AM_LDFLAGS = -lm

polkit_action_in_FILES += viewer/org.gnome.gzviewer.policy.in
desktop_in_FILES += viewer/gzviewer.desktop.in
pixmap_FILES += viewer/gzviewer.png

bin_PROGRAMS += gzviewer

gzviewer_SOURCES = \
	viewer/gzviewer.c \
	viewer/gzviewer_if.c \
	viewer/gzviewer.h

gzviewer_CFLAGS = $(AM_CFLAGS) $(GTK_CFLAGS)
gzviewer_LDADD = $(libzbc_ldadd) $(GTK_LIBS)

dist_man8_MANS += viewer/gzviewer.8

viewer/org.gnome.gzviewer.policy: viewer/org.gnome.gzviewer.policy.in Makefile
	$(bin_subst) < viewer/org.gnome.gzviewer.policy.in > viewer/org.gnome.gzviewer.policy

viewer/gzviewer.desktop: viewer/gzviewer.desktop.in Makefile
	$(bin_subst) < viewer/gzviewer.desktop.in > viewer/gzviewer.desktop
