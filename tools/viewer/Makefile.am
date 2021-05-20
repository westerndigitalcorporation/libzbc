# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

AM_LDFLAGS = -lm

bin_PROGRAMS += gzviewer

gzviewer_SOURCES = \
	viewer/gzviewer.c \
	viewer/gzviewer_if.c \
	viewer/gzviewer.h

gzviewer_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
gzviewer_LDADD = $(libzbc_ldadd) $(GTK_LIBS)

dist_man8_MANS += viewer/gzviewer.8
