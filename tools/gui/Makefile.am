# SPDX-License-Identifier: LGPL-3.0-or-later
# 
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

bin_PROGRAMS += gzbc

gzbc_SOURCES = \
	gui/gzbc.c \
	gui/gzbc_if.c \
	gui/gzbc_if_dev.c \
	gui/gzbc.h

gzbc_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
gzbc_LDADD = $(libzbc_ldadd) $(GTK_LIBS) -lpthread

dist_man8_MANS += gui/gzbc.8
