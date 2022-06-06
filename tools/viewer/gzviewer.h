/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital COrporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#ifndef __GZWATCH_H__
#define __GZWATCH_H__

#include <sys/time.h>
#include <gtk/gtk.h>

#include <libzbc/zbc.h>

/*
 * Device zone information.
 */
struct gzv_zone {
	unsigned int		zno;
	struct zbc_zone		*zbc_zone;
	GtkWidget		*da;
};

/*
 * GUI data.
 */
struct gzv {

	/*
	 * Parameters.
	 */
	int			refresh_interval;
	int			abort;

	/*
	 * For handling timer and signals.
	 */
	GSource			*refresh_timer;
	unsigned long long	last_refresh;
	int			sig_pipe[2];

	/*
	 * Interface stuff.
	 */
	GdkRGBA			conv_color;
	GdkRGBA			seqnw_color;
	GdkRGBA			seqw_color;
	GdkRGBA			black;
	GtkWidget		*window;
	GtkAdjustment 		*vadj;

	/*
	 * Device information.
	 */
	char			*path;
	struct zbc_device	*dev;
	struct zbc_device_info	info;
	unsigned int		nr_zones;
	unsigned int		nr_conv_zones;
	struct zbc_zone		*zbc_zones;

	/*
	 * Drawn zones.
	 */
	unsigned int		nr_row;
	unsigned int		nr_col;
	unsigned int		nr_grid_zones;
	unsigned int		max_row;
	struct gzv_zone		*grid_zones;
	unsigned int		grid_zno_first;
};

/**
 * System time in usecs.
 */
static inline unsigned long long gzv_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (unsigned long long) tv.tv_sec * 1000LL +
		(unsigned long long) tv.tv_usec / 1000;
}

extern struct gzv gzv;

int gzv_report_zones(unsigned int zno_start, unsigned int nr_zones);

void gzv_if_create(void);
void gzv_if_destroy(void);

#endif /* __GZWATCH_H__ */
