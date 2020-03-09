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
#ifndef __GZBC_H__
#define __GZBC_H__

#include <sys/time.h>
#include <pthread.h>
#include <gtk/gtk.h>

#include <libzbc/zbc.h>

/**
 * Default refresh interval (milliseconds).
 */
#define DZ_INTERVAL     1000

/**
 * Zone information list columns.
 */
enum {
	DZ_ZONE_NUM = 0,
	DZ_ZONE_TYPE,
	DZ_ZONE_COND,
	DZ_ZONE_RWP_RECOMMENDED,
	DZ_ZONE_NONSEQ,
	DZ_ZONE_START,
	DZ_ZONE_LENGTH,
	DZ_ZONE_WP,
	DZ_ZONE_VISIBLE,
	DZ_ZONE_LIST_COLUMS
};

/**
 * Device command IDs.
 */
enum {
	DZ_CMD_REPORT_ZONES,
	DZ_CMD_ZONE_OP,
};

/**
 * Maximum number of devices that can be open.
 */
#define DZ_MAX_DEV	32

/**
 * Device zone information.
 */
typedef struct dz_dev_zone {

	int			no;
	int			visible;
	struct zbc_zone		info;

} dz_dev_zone_t;

/**
 * GUI Tab data.
 */
typedef struct dz_dev {

	char			path[128];
	int			opening;

	struct zbc_device	*dev;
	struct zbc_device_info	info;
	int			block_size;
	int			use_hexa;

	int			zone_ro;
	unsigned int		zone_op;
	int			zone_no;
	unsigned int		max_nr_zones;
	unsigned int		nr_zones;
	struct zbc_zone		*zbc_zones;
	dz_dev_zone_t		*zones;

	/**
	 * Command execution.
	 */
	int			cmd_id;
	pthread_t		cmd_thread;
	GtkWidget		*cmd_dialog;

	/**
	 * Interface stuff.
	 */
	GtkWidget		*page;
	GtkWidget		*page_frame;

	GtkWidget		*zfilter_combo;
	GtkWidget		*zlist_frame_label;
	GtkWidget		*zlist_treeview;
	GtkTreeModel		*zlist_model;
	GtkListStore		*zlist_store;
	unsigned int		zlist_start_no;
	unsigned int		zlist_end_no;
	int			zlist_selection;
	GtkWidget		*znum_entry;
	GtkWidget		*zblock_entry;

	GtkWidget		*zones_da;

} dz_dev_t;

/**
 * GUI data.
 */
typedef struct dz {

	dz_dev_t		dev[DZ_MAX_DEV];
	int			nr_devs;

	int			interval;
	int			block_size;
	int			abort;

	/**
	 * Interface stuff.
	 */
	GtkWidget		*window;
	GtkWidget		*vbox;
	GtkWidget		*notebook;
	GtkWidget		*no_dev_frame;

	GdkRGBA			conv_color;
	GdkRGBA			seqnw_color;
	GdkRGBA			seqw_color;

	/**
	 * For handling timer and signals.
	 */
	guint			timer_id;
	int			sig_pipe[2];

} dz_t;

/**
 * System time in usecs.
 */
static inline unsigned long long dz_usec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (unsigned long long) tv.tv_sec * 1000000LL +
		(unsigned long long) tv.tv_usec;
}

extern dz_t dz;

extern dz_dev_t * dz_open(char *path);
extern void dz_close(dz_dev_t *dzd);

extern int dz_cmd_exec(dz_dev_t *dzd, int cmd_id, char *msg);

extern void dz_if_create(void);
extern void dz_if_destroy(void);
extern void dz_if_add_device(char *dev_path);
extern dz_dev_t * dz_if_dev_open(char *path);
extern void dz_if_dev_close(dz_dev_t *dzd);
extern void dz_if_dev_update(dz_dev_t *dzd, int do_report_zones);

#endif /* __GZBC_H__ */
