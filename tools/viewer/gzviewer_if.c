// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital COrporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include "gzviewer.h"

static void gzv_set_zone_tooltip(struct gzv_zone *zone)
{
	struct zbc_zone *zbcz = zone->zbc_zone;
	char info[512];

	if (!zbcz) {
		gtk_widget_set_has_tooltip(GTK_WIDGET(zone->da), false);
		gtk_widget_set_tooltip_markup(GTK_WIDGET(zone->da), NULL);
		return;
	}

	snprintf(info, sizeof(info) - 1,
		 "<b>Zone %u</b>:\n"
		 "  - Type: %s\n"
		 "  - Condition: %s\n"
		 "  - Start sector: %llu\n"
		 "  - Length: %llu 512-B sectors",
		 zone->zno, zbc_zone_type_str(zbcz->zbz_type),
		 zbc_zone_condition_str(zbcz->zbz_condition),
		 zbc_zone_start(zbcz), zbc_zone_length(zbcz));
	gtk_widget_set_tooltip_markup(GTK_WIDGET(zone->da), info);
	gtk_widget_set_has_tooltip(GTK_WIDGET(zone->da), true);
}

static void gzv_if_update(void)
{
	struct zbc_zone *zbcz;
	unsigned int i, z;

	if (gzv.grid_zno_first >= gzv.nr_zones)
		gzv.grid_zno_first = gzv.nr_zones / gzv.nr_col;

	if (gzv_report_zones(gzv.grid_zno_first, gzv.nr_grid_zones))
		goto out;

	z = gzv.grid_zno_first;
	for (i = 0; i < gzv.nr_grid_zones; i++) {
		gzv.grid_zones[i].zno = z;
		zbcz = gzv.grid_zones[i].zbc_zone;
		if (z >= gzv.nr_zones) {
			if (zbcz)
				gtk_widget_hide(gzv.grid_zones[i].da);
			gzv.grid_zones[i].zbc_zone = NULL;
		} else {
			gzv.grid_zones[i].zbc_zone = &gzv.zbc_zones[z];
			if (!zbcz)
				gtk_widget_show(gzv.grid_zones[i].da);
		}
		gzv_set_zone_tooltip(&gzv.grid_zones[i]);
		gtk_widget_queue_draw(gzv.grid_zones[i].da);
		z++;
	}

out:
	gzv.last_refresh = gzv_msec();
}

static gboolean gzv_if_timer_cb(gpointer user_data)
{
	if (gzv.last_refresh + gzv.refresh_interval <= gzv_msec())
		gzv_if_update();

	return TRUE;
}

static gboolean gzv_if_resize_cb(GtkWidget *widget, GdkEvent *event,
				gpointer user_data)
{
	gzv_if_update();

	return FALSE;
}

static void gzv_if_delete_cb(GtkWidget *widget, GdkEvent *event,
			    gpointer user_data)
{
	gzv.window = NULL;
	gtk_main_quit();
}

static gboolean gzv_if_zone_draw_cb(GtkWidget *widget, cairo_t *cr,
				   gpointer user_data)
{
	struct gzv_zone *zone= user_data;
	struct zbc_zone *z = zone->zbc_zone;
	GtkAllocation allocation;
	cairo_text_extents_t te;
	char str[16];
	long long w;

	/* Current size */
	gtk_widget_get_allocation(zone->da, &allocation);

	gtk_render_background(gtk_widget_get_style_context(widget),
			      cr, 0, 0, allocation.width, allocation.height);

	if (!z)
		return TRUE;

	/* Draw zone */
	if (zbc_zone_conventional(z))
		gdk_cairo_set_source_rgba(cr, &gzv.conv_color);
	else if (zbc_zone_full(z))
		gdk_cairo_set_source_rgba(cr, &gzv.seqw_color);
	else
		gdk_cairo_set_source_rgba(cr, &gzv.seqnw_color);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	if (!zbc_zone_conventional(z) &&
	    zbc_zone_wp(z) > zbc_zone_start(z)) {
		/* Written space in zone */
		w = (long long)allocation.width *
		     (zbc_zone_wp(z) - zbc_zone_start(z)) / zbc_zone_length(z);
		if (w > allocation.width)
			w = allocation.width;

		gdk_cairo_set_source_rgba(cr, &gzv.seqw_color);
		cairo_rectangle(cr, 0, 0, w, allocation.height);
		cairo_fill(cr);
	}

	/* Draw zone number */
	gdk_cairo_set_source_rgba(cr, &gzv.black);
	cairo_select_font_face(cr, "Monospace",
			       CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 10);
	sprintf(str, "%05d", zone->zno);
	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr, allocation.width / 2 - te.width / 2 - te.x_bearing,
		      (allocation.height + te.height) / 2);
	cairo_show_text(cr, str);

	return TRUE;
}

static gboolean gzv_if_scroll_cb(GtkWidget *widget, GdkEventScroll *scroll,
				gpointer user_data)
{
	unsigned int row = gtk_adjustment_get_value(gzv.vadj);
	unsigned int new_row = row;

	switch (scroll->direction) {
	case GDK_SCROLL_UP:
		if (row > 0)
			new_row = row - 1;
		break;
	case GDK_SCROLL_DOWN:
		if (row < gzv.max_row)
			new_row = row + 1;
		break;
	case GDK_SCROLL_LEFT:
	case GDK_SCROLL_RIGHT:
	case GDK_SCROLL_SMOOTH:
	default:
		break;
	}

	if (new_row != row)
		gtk_adjustment_set_value(gzv.vadj, new_row);

	return TRUE;
}

static gboolean gzv_if_scroll_value_cb(GtkWidget *widget,
				       GdkEvent *event, gpointer user_data)
{
	unsigned int zno, row = gtk_adjustment_get_value(gzv.vadj);

	if (row >= gzv.max_row)
		row = gzv.max_row - 1;

	zno = row * gzv.nr_col;
	if (zno != gzv.grid_zno_first) {
		gzv.grid_zno_first = zno;
		gzv_if_update();
	}

	return TRUE;
}

void gzv_if_create(void)
{
	GtkWidget *frame, *hbox, *scrollbar, *grid, *da;
	GtkAdjustment *vadj;
	struct gzv_zone *zone;
	unsigned int r, c, z = 0;
	char str[128];

	/* Get colors */
	gdk_rgba_parse(&gzv.conv_color, "Magenta");
	gdk_rgba_parse(&gzv.seqnw_color, "Green");
	gdk_rgba_parse(&gzv.seqw_color, "Red");
	gdk_rgba_parse(&gzv.black, "Black");

	/* Window */
	gzv.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(gzv.window), "ZBC Device Zone State");
	gtk_container_set_border_width(GTK_CONTAINER(gzv.window), 10);

	g_signal_connect((gpointer) gzv.window, "delete-event",
			 G_CALLBACK(gzv_if_delete_cb),
			 NULL);

	/* Create a top frame */
	if (!gzv.nr_conv_zones)
		snprintf(str, sizeof(str) - 1,
			 "<b>%s</b>: %u sequential zones",
			 gzv.path, gzv.nr_zones);
	else
		snprintf(str, sizeof(str) - 1,
			 "<b>%s</b>: %u zones (%u conventional + %u sequential)",
			 gzv.path, gzv.nr_zones, gzv.nr_conv_zones,
			 gzv.nr_zones - gzv.nr_conv_zones);
	frame = gtk_frame_new(str);
	gtk_container_add(GTK_CONTAINER(gzv.window), frame);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
	gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.05, 0.5);
	gtk_widget_show(frame);

	/* hbox for grid and scrollbar */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_show(hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
	gtk_container_add(GTK_CONTAINER(frame), hbox);

	/* Add a grid */
	grid = gtk_grid_new();
	gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), true);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), true);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
	gtk_box_pack_start(GTK_BOX(hbox), grid, TRUE, TRUE, 0);
	gtk_widget_show(grid);

	for (r = 0; r < gzv.nr_row; r++) {
		for (c = 0; c < gzv.nr_col; c++) {
			zone = &gzv.grid_zones[z];

			da = gtk_drawing_area_new();
			gtk_widget_set_size_request(da, 100, 60);
			gtk_widget_show(da);

			zone->da = da;
			gtk_grid_attach(GTK_GRID(grid), da, c, r, 1, 1);
			g_signal_connect(da, "draw",
					 G_CALLBACK(gzv_if_zone_draw_cb), zone);
			z++;
		}
	}

	/* Add scrollbar */
	vadj = gtk_adjustment_new(0, 0, gzv.max_row, 1, 1, gzv.nr_row);
	gzv.vadj = vadj;
	g_signal_connect(vadj, "value-changed",
			 G_CALLBACK(gzv_if_scroll_value_cb), NULL);

	scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vadj);
	gtk_widget_show(scrollbar);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);
	gtk_widget_add_events(scrollbar, GDK_SCROLL_MASK);

	gtk_widget_add_events(gzv.window, GDK_SCROLL_MASK);
	g_signal_connect(gzv.window, "scroll-event",
			 G_CALLBACK(gzv_if_scroll_cb), NULL);

	/* Finish setup */
	g_signal_connect(gzv.window, "configure-event",
			 G_CALLBACK(gzv_if_resize_cb), NULL);

	/* Add timer for automatic refresh */
	gzv.refresh_timer = g_timeout_source_new(gzv.refresh_interval);
	g_source_set_name(gzv.refresh_timer, "refresh-timer");
	g_source_set_can_recurse(gzv.refresh_timer, FALSE);
	g_source_set_callback(gzv.refresh_timer, gzv_if_timer_cb,
			      NULL, NULL);
	gzv.last_refresh = gzv_msec();
	g_source_attach(gzv.refresh_timer, NULL);

	gtk_widget_show_all(gzv.window);
	gzv_if_update();
}

void gzv_if_destroy(void)
{

	if (gzv.refresh_timer) {
		g_source_destroy(gzv.refresh_timer);
		gzv.refresh_timer = NULL;
	}

	if (gzv.window) {
		gtk_widget_destroy(gzv.window);
		gzv.window = NULL;
	}
}
