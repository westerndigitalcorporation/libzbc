// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <math.h>

#include "gzviewer.h"

/*
 * Device control.
 */
struct gzv gzv;

/*
 * Signal handling.
 */
static gboolean gzv_process_signal(GIOChannel *source,
				  GIOCondition condition,
				  gpointer user_data)
{
	char buf[32];
	ssize_t size;

	if (condition & G_IO_IN) {
		size = read(g_io_channel_unix_get_fd(source), buf, sizeof(buf));
		if (size > 0) {
			/* Got signal */
			gtk_main_quit();
			return TRUE;
		}
	}

	return FALSE;
}

static void gzv_sig_handler(int sig)
{
	/* Propagate signal through the pipe */
	if (write(gzv.sig_pipe[1], &sig, sizeof(int)) < 0)
		printf("Signal %d processing failed\n", sig);
}

static void gzv_set_signal_handlers(void)
{
	GIOChannel *sig_channel;
	long fd_flags;
	int ret;

	ret = pipe(gzv.sig_pipe);
	if (ret < 0) {
		perror("pipe");
		exit(1);
	}

	fd_flags = fcntl(gzv.sig_pipe[1], F_GETFL);
	if (fd_flags < 0) {
		perror("Read descriptor flags");
		exit(1);
	}
	ret = fcntl(gzv.sig_pipe[1], F_SETFL, fd_flags | O_NONBLOCK);
	if (ret < 0) {
		perror("Write descriptor flags");
		exit(1);
	}

	/* Install the unix signal handler */
	signal(SIGINT, gzv_sig_handler);
	signal(SIGQUIT, gzv_sig_handler);
	signal(SIGTERM, gzv_sig_handler);

	/* Convert the reading end of the pipe into a GIOChannel */
	sig_channel = g_io_channel_unix_new(gzv.sig_pipe[0]);
	g_io_channel_set_encoding(sig_channel, NULL, NULL);
	g_io_channel_set_flags(sig_channel,
			       g_io_channel_get_flags(sig_channel) |
			       G_IO_FLAG_NONBLOCK,
			       NULL);
	g_io_add_watch(sig_channel,
		       G_IO_IN | G_IO_PRI,
		       gzv_process_signal, NULL);
}

/*
 * Close a device.
 */
static void gzv_close(void)
{

	if (!gzv.dev)
		return;

	zbc_close(gzv.dev);
	free(gzv.zbc_zones);
	free(gzv.grid_zones);
	gzv.dev = NULL;
}

static char *gzv_choose_dev(void)
{
	GtkFileChooser *chooser;
	GtkFileFilter *filter;
	GtkWidget *dialog;
	char *path = NULL;
	gint res;

	/* File chooser */
	dialog = gtk_file_chooser_dialog_new("Open Zoned Block Device",
					     GTK_WINDOW(gzv.window),
					     GTK_FILE_CHOOSER_ACTION_OPEN,
					     "_Cancel", GTK_RESPONSE_CANCEL,
					     "_Open", GTK_RESPONSE_ACCEPT,
					     NULL);

	chooser = GTK_FILE_CHOOSER(dialog);
	gtk_file_chooser_set_current_folder(chooser, "/dev");

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Block Device Files");
	gtk_file_filter_add_mime_type(filter, "inode/blockdevice");
	gtk_file_chooser_add_filter(chooser, filter);

	res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT)
		path = gtk_file_chooser_get_filename(chooser);

	gtk_widget_destroy(dialog);

	return path;
}

/*
 * Open a device.
 */
static int gzv_open(void)
{
	unsigned int i;
	int ret;

	/* Open device file */
	ret = zbc_open(gzv.path, O_RDONLY, &gzv.dev);
	if (ret)
		return ret;

	zbc_get_device_info(gzv.dev, &gzv.info);

	/* Get list of all zones */
	ret = zbc_list_zones(gzv.dev, 0, ZBC_RO_ALL,
			     &gzv.zbc_zones, &gzv.nr_zones);
	if (ret != 0)
		goto out;

	for (i = 0; i < gzv.nr_zones; i++) {
		if (zbc_zone_conventional(&gzv.zbc_zones[i]))
			gzv.nr_conv_zones++;
	}

	/* Set defaults */
	if (!gzv.nr_col && !gzv.nr_row && gzv.nr_zones < 100) {
		gzv.nr_col = sqrt(gzv.nr_zones);
		gzv.nr_row = (gzv.nr_zones + gzv.nr_col - 1) / gzv.nr_col;
	} else {
		if (!gzv.nr_col)
			gzv.nr_col = 10;
		if (!gzv.nr_row)
			gzv.nr_row = 10;
	}
	gzv.max_row = (gzv.nr_zones + gzv.nr_col - 1) / gzv.nr_col;

	/* Allocate zone array */
	gzv.nr_grid_zones = gzv.nr_col * gzv.nr_row;
	gzv.grid_zones = calloc(gzv.nr_grid_zones, sizeof(struct gzv_zone));
	if (!gzv.grid_zones) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < gzv.nr_grid_zones && i < gzv.nr_zones; i++) {
		gzv.grid_zones[i].zno = i;
		gzv.grid_zones[i].zbc_zone = &gzv.zbc_zones[i];
	}

out:
	if (ret)
		gzv_close();

	return ret;
}

int main(int argc, char **argv)
{
	gboolean init_ret;
	gboolean verbose = FALSE;
	GError *error = NULL;
	int ret = 0;
	GOptionEntry options[] = {
		{
			"interval", 'i', 0,
			G_OPTION_ARG_INT64, &gzv.refresh_interval,
			"Refresh interval (milliseconds)", NULL
		},
		{
			"width", 'w', 0,
			G_OPTION_ARG_INT, &gzv.nr_col,
			"Number of zones per row (default: 10)", NULL
		},
		{
			"height", 'h', 0,
			G_OPTION_ARG_INT, &gzv.nr_row,
			"Number of zone rows (default: 10)", NULL
		},
		{
			"verbose", 'v', 0,
			G_OPTION_ARG_NONE, &verbose,
			"Set libzbc verbose mode", NULL
		},
		{ NULL }
	};

	/* Init */
	memset(&gzv, 0, sizeof(gzv));
	init_ret = gtk_init_with_args(&argc, &argv,
				      "ZBC device zone state GUI",
				      options, NULL, &error);
	if (init_ret == FALSE ||
	    error != NULL) {
		printf("Failed to parse command line arguments: %s\n",
		       error->message);
		g_error_free(error);
		return 1;
	}

	if (gzv.refresh_interval < 0) {
		fprintf(stderr, "Invalid update interval\n");
		return 1;
	}

	if (verbose)
		zbc_set_log_level("debug");

	/* Set default values and open the device */
	if (!gzv.refresh_interval)
		gzv.refresh_interval = 500;

	/* Create the main window */
	gzv_if_create_window();

	/* Check user credentials */
	if (getuid() != 0) {
		gzv_if_err("Root privileges are required for running gzviewer",
			   "Opening a block device file can only be done with "
			   "elevated priviledges");
		ret = 1;
		goto out;
	}

	if (argc < 2) {
		/* No device specified: use the file chooser */
		gzv.path = gzv_choose_dev();
		if (!gzv.path) {
			gzv_if_err("No device specified",
				"Specifying a zoned block device is mandatory");
			fprintf(stderr, "No device specified\n");
			ret = 1;
			goto out;
		}
	} else {
		gzv.path = argv[1];
	}

	if (gzv_open()) {
		ret = errno;
		gzv_if_err("Open device failed",
			   "Opening %s generated error %d (%s)",
			   gzv.path, ret, strerror(ret));
		fprintf(stderr, "Open device %s failed %d (%s)\n",
			gzv.path, ret, strerror(ret));
		ret = 1;
		goto out;
	}

	gzv_set_signal_handlers();

	/* Create GUI */
	gzv_if_create();

	/* Main event loop */
	gtk_main();

out:
	/* Cleanup GUI */
	gzv_if_destroy();

	return ret;
}

/*
 * Report zones.
 */
int gzv_report_zones(unsigned int zno_start, unsigned int nr_zones)
{
	unsigned int nrz;
	int ret;

	if (zno_start >= gzv.nr_zones)
		return 0;

	nrz = nr_zones;
	if (zno_start + nrz > gzv.nr_zones)
		nrz = gzv.nr_zones - zno_start;

	/* Get zone information */
	ret = zbc_report_zones(gzv.dev,
			       zbc_zone_start(&gzv.zbc_zones[zno_start]),
			       ZBC_RO_ALL, &gzv.zbc_zones[zno_start],
			       &nrz);
	if (ret) {
		fprintf(stderr, "Get zone information failed %d (%s)\n",
			errno, strerror(errno));
		return ret;
	}

	return 0;
}

