# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

bin_PROGRAMS += zbc_open_zone

zbc_open_zone_SOURCES = open_zone/zbc_open_zone.c
zbc_open_zone_LDADD = libzone_op.la $(libzbc_ldadd)

dist_man8_MANS += open_zone/zbc_open_zone.8
