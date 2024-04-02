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
#include <signal.h>
#include <fcntl.h>

#include "gzbc.h"

static void dz_if_show_nodev(void);
static void dz_if_hide_nodev(void);

static gboolean dz_if_resize_cb(GtkWidget *widget, GdkEvent *event,
				gpointer user_data);
static void dz_if_delete_cb(GtkWidget *widget, GdkEvent *event,
			    gpointer user_data);
static void dz_if_open_cb(GtkWidget *widget, gpointer user_data);
static void dz_if_close_cb(GtkWidget *widget, gpointer user_data);
static void dz_if_close_page_cb(GtkWidget *widget, gpointer user_data);
static void dz_if_exit_cb(GtkWidget *widget, gpointer user_data);
static gboolean dz_if_timer_cb(gpointer user_data);

void dz_if_err(const char *msg, const char *fmt, ...)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s", msg);
	if (fmt) {
		va_list args;
		char secondary[256];

		va_start(args, fmt);
		vsnprintf(secondary, 255, fmt, args);
		va_end(args);

		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG(dialog),
			 "%s", secondary);
	}

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void dz_if_create(void)
{
	GtkWidget *toolbar;
	GtkWidget *sep;
	GtkToolItem *ti;

	/* Get colors */
	gdk_rgba_parse(&dz.conv_color, "Magenta");
	gdk_rgba_parse(&dz.seqnw_color, "Green");
	gdk_rgba_parse(&dz.seqw_color, "Red");

	/* Window */
	dz.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(dz.window), "ZBC Device Zone State");
	gtk_container_set_border_width(GTK_CONTAINER(dz.window), 10);

	g_signal_connect((gpointer) dz.window, "delete-event",
			 G_CALLBACK(dz_if_delete_cb),
			 NULL);

	/* Top vbox */
	dz.vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_show(dz.vbox);
	gtk_container_add(GTK_CONTAINER(dz.window), dz.vbox);

	/* Toolbar */
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_widget_show(toolbar);
	gtk_box_pack_start(GTK_BOX(dz.vbox), toolbar, FALSE, FALSE, 0);

	/* Toolbar open button */
	ti = gtk_tool_button_new(gtk_image_new_from_icon_name("document-open",
					GTK_ICON_SIZE_LARGE_TOOLBAR), "Open");
	gtk_tool_item_set_tooltip_text(ti, "Open a device");
	gtk_tool_item_set_is_important(ti, TRUE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ti, -1);
	g_signal_connect(G_OBJECT(ti), "clicked",
			 G_CALLBACK(dz_if_open_cb), NULL);

	/* Toolbar close button */
	ti = gtk_tool_button_new(gtk_image_new_from_icon_name("window-close",
					GTK_ICON_SIZE_LARGE_TOOLBAR), "Close");
	gtk_tool_item_set_tooltip_text(ti, "Close current device");
	gtk_tool_item_set_is_important(ti, TRUE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ti, -1);
	g_signal_connect(G_OBJECT(ti), "clicked",
			 G_CALLBACK(dz_if_close_cb), NULL);

	/* Separator */
	ti = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ti, -1);

	/* Toolbar exit button */
	ti = gtk_tool_button_new(gtk_image_new_from_icon_name("application-exit",
					GTK_ICON_SIZE_LARGE_TOOLBAR), "Quit");
	gtk_tool_item_set_tooltip_text(ti, "Quit");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ti, -1);
	g_signal_connect(G_OBJECT(ti), "clicked",
			 G_CALLBACK(dz_if_exit_cb), NULL);

	/* Separator */
	sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show(sep);
	gtk_box_pack_start(GTK_BOX(dz.vbox), sep, FALSE, FALSE, 0);

	/* Initially, no device open: show "no device" frame */
	dz_if_show_nodev();

	/* Add timer for automatic refresh */
	if (dz.interval >= DZ_INTERVAL)
		dz.timer_id = g_timeout_add(dz.interval, dz_if_timer_cb, NULL);

	/* Finish setup */
	g_signal_connect((gpointer) dz.window, "configure-event",
			 G_CALLBACK(dz_if_resize_cb), NULL);

	gtk_window_set_default_size(GTK_WINDOW(dz.window), 1024, 768);
	gtk_widget_show_all(dz.window);
}

void dz_if_destroy(void)
{

	if ( dz.timer_id ) {
		g_source_remove(dz.timer_id);
		dz.timer_id = 0;
	}

	if ( dz.window ) {
		gtk_widget_destroy(dz.window);
		dz.window = NULL;
	}
}

void dz_if_add_device(char *dev_path)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;
	dz_dev_t *dzd;
	int page_no;
	char str[256];

	/* Open device */
	dzd = dz_if_dev_open(dev_path);
	if (!dzd)
		return;

	dz_if_hide_nodev();

	/* Add page */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

	snprintf(str, sizeof(str) - 1, "<b>%s</b>", dzd->path);
	label = gtk_label_new(NULL);
	gtk_label_set_text(GTK_LABEL(label), str);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	button = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect((gpointer) button, "clicked",
			 G_CALLBACK(dz_if_close_page_cb), dzd);

	gtk_widget_show_all(hbox);

	page_no = gtk_notebook_append_page(GTK_NOTEBOOK(dz.notebook),
					   dzd->page_frame, hbox);
	dzd->page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(dz.notebook), page_no);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(dz.notebook), page_no);
}

static void dz_if_show_nodev(void)
{

	if (dz.notebook) {
		/* Remove notebook */
		gtk_widget_destroy(dz.notebook);
		dz.notebook = NULL;
	}

	if (!dz.no_dev_frame) {

		GtkWidget *frame;
		GtkWidget *label;

		frame = gtk_frame_new(NULL);
		gtk_widget_show(frame);
		gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
		gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
		gtk_box_pack_start(GTK_BOX(dz.vbox), frame, TRUE, TRUE, 0);

		label = gtk_label_new(NULL);
		gtk_widget_show(label);
		gtk_label_set_text(GTK_LABEL(label), "<b>No device open</b>");
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_container_add(GTK_CONTAINER(frame), label);

		dz.no_dev_frame = frame;

	}
}

static void dz_if_hide_nodev(void)
{

	if (dz.no_dev_frame) {
		/* Remove "no device" frame */
		gtk_widget_destroy(dz.no_dev_frame);
		dz.no_dev_frame = NULL;
	}

	if (!dz.notebook) {
		/* Create the notebook */
		dz.notebook = gtk_notebook_new();
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(dz.notebook), GTK_POS_TOP);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(dz.notebook), TRUE);
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(dz.notebook), TRUE);
		gtk_widget_show(dz.notebook);
		gtk_box_pack_start(GTK_BOX(dz.vbox), dz.notebook, TRUE, TRUE, 0);
	}
}

static void dz_if_remove_device(dz_dev_t *dzd)
{
	int page_no = gtk_notebook_page_num(GTK_NOTEBOOK(dz.notebook), dzd->page_frame);

	/* Close the device */
	dz_if_dev_close(dzd);

	/* Remove the page */
	gtk_notebook_remove_page(GTK_NOTEBOOK(dz.notebook), page_no);
	dzd->page = NULL;

	if (dz.nr_devs == 0)
		/* Show "no device" */
		dz_if_show_nodev();
}

static dz_dev_t *dz_if_get_device(void)
{
	dz_dev_t *dzd = NULL;
	GtkWidget *page = NULL;
	int i;

	if (!dz.notebook)
		return NULL;

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(dz.notebook),
			 gtk_notebook_get_current_page(GTK_NOTEBOOK(dz.notebook)));
	for (i = 0; i < DZ_MAX_DEV; i++) {
		dzd = &dz.dev[i];
		if (dzd->dev && dzd->page == page)
			return dzd;
	}

	return NULL;
}

static void dz_if_open_cb(GtkWidget *widget, gpointer user_data)
{
	GtkFileChooser *chooser;
	GtkFileFilter *filter;
	GtkWidget *dialog;
	char *dev_path = NULL;
	gint res;

	/* File chooser */
	dialog = gtk_file_chooser_dialog_new("Open Zoned Block Device",
					      GTK_WINDOW(dz.window),
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      "_Cancel", GTK_RESPONSE_CANCEL,
					      "_Open", GTK_RESPONSE_ACCEPT,
					      NULL);

	chooser = GTK_FILE_CHOOSER(dialog);
	gtk_file_chooser_set_current_folder(chooser, "/dev/");

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Block Device Files");
	gtk_file_filter_add_mime_type(filter, "inode/blockdevice");
	gtk_file_chooser_add_filter(chooser, filter);

	res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT)
		dev_path = gtk_file_chooser_get_filename(chooser);

	gtk_widget_destroy(dialog);

	if (dev_path) {
		dz_if_add_device(dev_path);
		g_free(dev_path);
	}
}

static void dz_if_close_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = dz_if_get_device();

	if (dzd)
		dz_if_remove_device(dzd);
}

static void dz_if_close_page_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;

	if (dzd)
		dz_if_remove_device(dzd);
}

static void dz_if_exit_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd;
	int i;

	if (dz.notebook) {
		for (i = 0; i < DZ_MAX_DEV; i++) {
			dzd = &dz.dev[i];
			if (dzd->dev)
				dz_if_remove_device(dzd);
		}
	}

	gtk_main_quit();
}

static gboolean dz_if_timer_cb(gpointer user_data)
{
	dz_dev_t *dzd = dz_if_get_device();

	if (dzd)
		dz_if_dev_update(dzd, 1);

	return TRUE;
}

static gboolean dz_if_resize_cb(GtkWidget *widget, GdkEvent *event,
				gpointer user_data)
{
	dz_dev_t *dzd = dz_if_get_device();

	if (dzd)
		dz_if_dev_update(dzd, 0);

	return FALSE;
}

static void dz_if_delete_cb(GtkWidget *widget, GdkEvent *event,
			    gpointer user_data)
{
	dz_dev_t *dzd;
	int i;

	dz.window = NULL;

	if (dz.notebook) {
		for (i = 0; i < DZ_MAX_DEV; i++) {
			dzd = &dz.dev[i];
			if (dzd->dev)
				dz_if_remove_device(dzd);
		}
	}

	gtk_main_quit();
}
