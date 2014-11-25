/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc.  This software is distributed
 * under the terms of the GNU Lesser General Public License version 3,
 * or any later version, "as is," without technical support, and WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  You should have received a copy
 * of the GNU Lesser General Public License along with libzbc.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: Damien Le Moal (damien.lemoal@hgst.com)
 *          Christophe Louargant (christophe.louargant@hgst.com)
 */

/***** Including files *****/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "gzbc.h"

/***** Declaration of private functions *****/

static gboolean
dz_if_zstate_draw_legend_cb(GtkWidget *widget,
			    cairo_t *cr,
			    gpointer user_data);

static gboolean
dz_if_zstate_draw_cb(GtkWidget *widget,
		     cairo_t *cr,
		     gpointer user_data);

static gboolean
dz_if_zinfo_scroll_cb(GtkWidget *widget,
		      GdkEvent *event,
		      gpointer user_data);

static gboolean
dz_if_zinfo_update_cb(GtkWidget *widget,
		      GdkEvent *event,
		      gpointer user_data);

static void
dz_if_delete_cb(GtkWidget *widget,
                GdkEvent *event,
                gpointer user_data);

static gboolean
dz_if_exit_cb(GtkWidget *widget,
              GdkEvent *event,
              gpointer user_data);

static gboolean
dz_if_timer_cb(gpointer user_data);

static gboolean
dz_if_refresh_cb(GtkWidget *widget,
                 GdkEvent *event,
                 gpointer user_data);

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data);

static void
dz_if_create_zinfo(int nr_lines);

static void
dz_if_update_zinfo();

static void
dz_if_refresh_zones(void);

/***** Definition of public functions *****/

int
dz_if_create(void)
{
    GtkWidget *win_vbox, *hbox, *ctrl_hbox, *vbox;
    GtkWidget *scrolledwindow, *viewport;
    GtkWidget *scrollbar;
    GtkWidget *window;
    GtkWidget *frame, *alignment;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *spinbutton;
    GtkWidget *hbuttonbox;
    GtkWidget *da;
    char str[128];

    /* Get colors */
    gdk_rgba_parse(&dz.conv_color, "Magenta");
    gdk_rgba_parse(&dz.seqw_color, "Red");
    gdk_rgba_parse(&dz.seqnw_color, "Dark Green");

    /* Window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ZBC Device Zone State");
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_resize(GTK_WINDOW(window), 720, 480);
    dz.window = window;

    g_signal_connect((gpointer) window, "delete-event",
                     G_CALLBACK(dz_if_delete_cb),
                     NULL);

    /* Top vbox */
    win_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_show(win_vbox);
    gtk_container_add(GTK_CONTAINER(window), win_vbox);

    /* Zone information frame */
    snprintf(str, sizeof(str) - 1, "<b>%s: %d zones</b>", dz.path, dz.nr_zones);
    frame = gtk_frame_new(str);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(win_vbox), frame, TRUE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);
    dz.zinfo_frame_label = gtk_frame_get_label_widget(GTK_FRAME(frame));
    gtk_label_set_use_markup(GTK_LABEL(dz.zinfo_frame_label), TRUE);

    alignment = gtk_alignment_new(1.0, 1.0, 1.0, 1.0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(frame), alignment);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 10, 10, 10, 10);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
    gtk_widget_show(scrolledwindow);
    gtk_box_pack_start(GTK_BOX(hbox), scrolledwindow, TRUE, TRUE, 0);

    dz.zinfo_vadj = gtk_adjustment_new(0, 0, dz.nr_zones, 1, DZ_ZONE_INFO_LINE_NUM, DZ_ZONE_INFO_LINE_NUM);
    scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, dz.zinfo_vadj);
    gtk_widget_show(scrollbar);
    gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

    g_signal_connect((gpointer) dz.zinfo_vadj, "value_changed",
		     G_CALLBACK(dz_if_zinfo_update_cb),
		     NULL);

    viewport = gtk_viewport_new(NULL, NULL);
    gtk_widget_set_size_request(viewport, -1, 300);
    gtk_widget_show(viewport);
    gtk_container_set_border_width(GTK_CONTAINER(viewport), 10);
    gtk_widget_add_events(viewport, GDK_SCROLL_MASK);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);
    dz.zinfo_viewport = viewport;

    g_signal_connect((gpointer) viewport, "scroll-event",
		     G_CALLBACK(dz_if_zinfo_scroll_cb),
		     NULL);

    /* Create info lines */
    dz_if_create_zinfo(DZ_ZONE_INFO_LINE_NUM);

    /* Zone use state drawing frame */
    frame = gtk_frame_new("<b>Zone State</b>");
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(win_vbox), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);

    alignment = gtk_alignment_new(1.0, 1.0, 1.0, 1.0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(frame), alignment);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 10, 10, 10, 10);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(alignment), vbox);

    da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, -1, 20);
    gtk_widget_show(da);
    gtk_container_add(GTK_CONTAINER(vbox), da);

    g_signal_connect((gpointer) da, "draw",
                     G_CALLBACK(dz_if_zstate_draw_legend_cb),
                     NULL);

    da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, -1, 100);
    gtk_widget_show(da);
    gtk_container_add(GTK_CONTAINER(vbox), da);
    dz.zstate_da = da;

    g_signal_connect((gpointer) da, "draw",
                     G_CALLBACK(dz_if_zstate_draw_cb),
                     NULL);

    /* Controls frame */
    frame = gtk_frame_new(NULL);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(win_vbox), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    alignment = gtk_alignment_new(1.0, 1.0, 1.0, 1.0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(frame), alignment);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 10, 10, 10, 10);

    /* Hbox for controls */
    ctrl_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(ctrl_hbox);
    gtk_container_add(GTK_CONTAINER(alignment), ctrl_hbox);

    /* Reset zone control */
    label = gtk_label_new("<b>Zone index</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), label, FALSE, FALSE, 0);

    spinbutton = gtk_spin_button_new_with_range(0, dz.nr_zones - 1, 1);
    gtk_widget_show(spinbutton);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), TRUE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinbutton), 0);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), spinbutton, FALSE, FALSE, 0);

    /* Zone control button Box */
    hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show(hbuttonbox);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), 10);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_START);
    gtk_box_set_spacing(GTK_BOX(hbuttonbox), 10);

    /* Reset button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);

    alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(button), alignment);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);

    image = gtk_image_new_from_icon_name("gtk-clear", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Reset Write Pointer");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_reset_cb),
                      spinbutton);

    /* Exit, refresh, ... button Box */
    hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show(hbuttonbox);
    gtk_box_pack_end(GTK_BOX(ctrl_hbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), 10);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbuttonbox), 10);

    /* Refresh button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);
    gtk_widget_set_can_default(button, TRUE);
    
    alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(button), alignment);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);
    
    image = gtk_image_new_from_icon_name("gtk-refresh", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    
    label = gtk_label_new_with_mnemonic("Refresh");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    g_signal_connect((gpointer) button, "button_press_event",
		     G_CALLBACK(dz_if_refresh_cb),
		     &dz);

    /* Exit button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);
    gtk_widget_set_can_default(button, TRUE);

    alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(button), alignment);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);

    image = gtk_image_new_from_icon_name("gtk-quit", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Exit");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_exit_cb),
                      &dz);

    if ( dz.interval > DZ_INTERVAL ) {
        /* Add timer for automatic refresh */
        dz.timer_id = g_timeout_add(dz.interval, dz_if_timer_cb, NULL);
    }

    g_signal_connect((gpointer) dz.window, "configure-event",
		     G_CALLBACK(dz_if_zinfo_update_cb),
		     NULL);

    /* Done */
    gtk_widget_show_all(dz.window);

    /* Update drawing area */
    dz_if_refresh_zones();

    return( 0 );

}

void
dz_if_destroy(void)
{

    if ( dz.timer_id ) {
        g_source_remove(dz.timer_id);
        dz.timer_id = 0;
    }

    if ( dz.window ) {
        gtk_widget_destroy(dz.window);
        dz.window = NULL;
    }

    return;

}

/***** Definition of private functions *****/

static char *dz_info_label[DZ_ZONE_INFO_FIELD_NUM] =
    {
        "Type",
        "Condition",
        "Need Reset",
        "Non Seq",
        "Start LBA",
        "Length",
        "Write pointer LBA"
    };

static void
dz_if_create_zinfo(int nr_lines)
{
    GtkWidget *grid = dz.zinfo_grid;
    GtkWidget *entry;
    GtkWidget *label;
    char str[64];
    int i;

    if ( nr_lines > (int)dz.nr_zones ) {
	nr_lines = dz.nr_zones;
    }

    if ( grid && (nr_lines == dz.zinfo_nr_lines) ) {
	return;
    }

    /* Cleanup */
    if ( grid ) {
	gtk_widget_destroy(grid);
    }

    if ( dz.zinfo_lines ) {
	free(dz.zinfo_lines);
	dz.zinfo_lines = NULL;
	dz.zinfo_nr_lines = 0;
    }

    /* Allocate zone information lines */
    dz.zinfo_lines = (dz_zinfo_line_t *) malloc(sizeof(dz_zinfo_line_t) * nr_lines);
    if ( ! dz.zinfo_lines ) {
	fprintf(stderr, "No memory\n");
	return;
    }
    memset(dz.zinfo_lines, 0, sizeof(dz_zinfo_line_t) * nr_lines);

    /* Re-create zone list */
    dz.zinfo_zno = 0;
    gtk_adjustment_configure(dz.zinfo_vadj,
			     dz.zinfo_zno,
			     0, dz.nr_zones,
			     1, nr_lines, nr_lines);
    dz.zinfo_nr_lines = nr_lines;

    grid = gtk_grid_new();
    gtk_widget_show(grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, FALSE);
    gtk_container_add(GTK_CONTAINER(dz.zinfo_viewport), grid);
    dz.zinfo_grid = grid;

    /* Info labels (grid top line) */
    for(i = 0; i < DZ_ZONE_INFO_FIELD_NUM; i++) {
	sprintf(str, "<b>%s</b>", dz_info_label[i]);
	label = gtk_label_new(str);
	gtk_label_set_text(GTK_LABEL(label), str);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_vexpand(label, FALSE);
	gtk_widget_show(label);
	gtk_grid_attach(GTK_GRID(grid), label, i + 1, 0, 1, 1);
    }

    /* Info lines */
    for(i = 0; i < dz.zinfo_nr_lines; i++) {

	/* Zone number label (grid left column) */
	sprintf(str, "<b>%05d</b>", i);
	label = gtk_label_new(str);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_set_hexpand(label, FALSE);
	gtk_widget_set_vexpand(label, FALSE);
	gtk_widget_show(label);
	gtk_grid_attach(GTK_GRID(grid), label, 0, i + 1, 1, 1);
	dz.zinfo_lines[i].label = label;

	/* Zone type */
	entry = gtk_entry_new();
	sprintf(str, "0x%01x", dz.zones[i].zbz_type);
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 1, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[0] = entry;

	/* Zone condition */
	entry = gtk_entry_new();
        sprintf(str, "0x%01x", dz.zones[i].zbz_condition);
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 2, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[1] = entry;

	/* Zone need reset */
	entry = gtk_entry_new();
        sprintf(str, "%s", dz.zones[i].zbz_need_reset ? "true" : "false");
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 3, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[2] = entry;

	/* Zone non seq */
	entry = gtk_entry_new();
        sprintf(str, "%s", dz.zones[i].zbz_non_seq? "true" : "false");
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 4, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[3] = entry;

	/* Zone start LBA */
	entry = gtk_entry_new();
        sprintf(str, "%llu", zbc_zone_start_lba(&dz.zones[i]));
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 5, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[4] = entry;

	/* Zone length */
	entry = gtk_entry_new();
        sprintf(str, "%llu", zbc_zone_length(&dz.zones[i]));
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 6, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[5] = entry;

	/* Zone wp LBA */
	entry = gtk_entry_new();
        sprintf(str, "%llu", zbc_zone_wp_lba(&dz.zones[i]));
        gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, FALSE);
	gtk_widget_show(entry);
	gtk_grid_attach(GTK_GRID(grid), entry, 7, i + 1, 1, 1);
	dz.zinfo_lines[i].entry[6] = entry;

    }

    return;
    
}

static void
dz_if_update_zinfo(void)
{
    GtkWidget *label, *entry;
    GdkRGBA color;
    struct zbc_zone *z;
    char str[64];
    int i;

    /* Info lines */
    for(i = 0; i < dz.zinfo_nr_lines; i++) {

	z = &dz.zones[dz.zinfo_zno + i];

	/* Zone index label (grid left column) */
	label = dz.zinfo_lines[i].label;
	sprintf(str, "<b>%05d</b>", dz.zinfo_zno + i);
	gtk_label_set_text(GTK_LABEL(label), str);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);

	/* Zone type */
	entry = dz.zinfo_lines[i].entry[0];
	if ( zbc_zone_conventional(z) ) {
	    strcpy(str, "Conventional");
	} else if ( zbc_zone_sequential_req(z) ) {
	    strcpy(str, "Seq write req.");
	} else if ( zbc_zone_sequential_pref(z) ) {
	    strcpy(str, "Seq write pref.");
	} else {
	    sprintf(str, "??? (0x%01x)", z->zbz_type);
	}
	gdk_rgba_parse(&color, "Black");
	gtk_widget_override_color(entry, GTK_STATE_NORMAL, &color);
        gtk_entry_set_text(GTK_ENTRY(entry), str);

	/* Zone condition */
	entry = dz.zinfo_lines[i].entry[1];
	gdk_rgba_parse(&color, "Black");
	if ( zbc_zone_not_wp(z) ) {
	    strcpy(str, "Not WP");
	} else if ( zbc_zone_empty(z) ) {
	    gdk_rgba_parse(&color, "Green");
	    strcpy(str, "Empty");
	} else if ( zbc_zone_full(z) ) {
	    gdk_rgba_parse(&color, "Red");
	    strcpy(str, "Full");
	} else if ( zbc_zone_open(z) ) {
	    strcpy(str, "Open");
	} else if ( zbc_zone_rdonly(z) ) {
	    strcpy(str, "Read-only");
	} else if ( zbc_zone_offline(z) ) {
	    strcpy(str, "Offline");
	} else {
	    sprintf(str, "??? (0x%01x)", z->zbz_condition);
	}
	gtk_widget_override_color(entry, GTK_STATE_NORMAL, &color);
        gtk_entry_set_text(GTK_ENTRY(entry), str);

	/* Zone need reset */
	entry = dz.zinfo_lines[i].entry[2];
	if ( zbc_zone_need_reset(z) ) {
	    gdk_rgba_parse(&color, "Red");
	    strcpy(str, "Yes");
	} else {
	    gdk_rgba_parse(&color, "Green");
	    strcpy(str, "No");
	}
	gtk_widget_override_color(entry, GTK_STATE_NORMAL, &color);
        gtk_entry_set_text(GTK_ENTRY(entry), str);

	/* Zone non seq */
	entry = dz.zinfo_lines[i].entry[3];
	if ( zbc_zone_non_seq(z) ) {
	    strcpy(str, "Yes");
	} else {
	    strcpy(str, "No");
	}
	gdk_rgba_parse(&color, "Black");
	gtk_widget_override_color(entry, GTK_STATE_NORMAL, &color);
        gtk_entry_set_text(GTK_ENTRY(entry), str);

	/* Zone start LBA */
	entry = dz.zinfo_lines[i].entry[4];
        sprintf(str, "%llu", zbc_zone_start_lba(z));
        gtk_entry_set_text(GTK_ENTRY(entry), str);

	/* Zone length */
	entry = dz.zinfo_lines[i].entry[5];
        sprintf(str, "%llu", zbc_zone_length(z));
        gtk_entry_set_text(GTK_ENTRY(entry), str);

	/* Zone wp LBA */
	entry = dz.zinfo_lines[i].entry[6];
	if ( zbc_zone_not_wp(z) ) {
	    strcpy(str, "N/A");
	    gdk_rgba_parse(&color, "Grey");
	    gtk_widget_override_color(entry, GTK_STATE_NORMAL, &color);
	} else {
	    sprintf(str, "%llu", zbc_zone_wp_lba(z));
	}
        gtk_entry_set_text(GTK_ENTRY(entry), str);

    }

    if ( ! dz.zinfo_line_height ) {
	GtkAllocation allocation;
	gtk_widget_get_allocation(dz.zinfo_lines[0].label, &allocation);
	dz.zinfo_line_height = allocation.height;
    }

    return;

}

static void
dz_if_refresh_zinfo(void)
{
    int zstart;

    /* Get current position in the list (first zone to show) */
    zstart = (int) gtk_adjustment_get_value(dz.zinfo_vadj);
    if ( (zstart + dz.zinfo_nr_lines) > (int)dz.nr_zones ) {
	zstart = dz.nr_zones - dz.zinfo_nr_lines;
    }
    dz.zinfo_zno = zstart;

    /* Update list view */
    dz_if_update_zinfo();

    /* Re-draw zone state */
    gtk_widget_queue_draw(dz.zstate_da);

    return;

}

static void
dz_if_refresh_zones(void)
{
    GtkWidget *dialog;
    unsigned int nr_zones = dz.nr_zones;
    char str[128];
    int ret;

    /* Refresh zone list */
    ret = dz_get_zones();
    if ( ret != 0 ) {

        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Get zone list failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Get zone list failed %d (%s)",
                                                 errno,
                                                 strerror(errno));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

	return;

    }

    if ( dz.nr_zones != nr_zones ) {

	/* Number of zones changed... */
	snprintf(str, sizeof(str) - 1, "<b>%s: %d zones</b>", dz.path, dz.nr_zones);
	gtk_label_set_text(GTK_LABEL(dz.zinfo_frame_label), str);
	gtk_label_set_use_markup(GTK_LABEL(dz.zinfo_frame_label), TRUE);
	
	if ( dz.zinfo_grid ) {
	    gtk_widget_destroy(dz.zinfo_grid);
	    dz.zinfo_grid = NULL;
	}

	dz.zinfo_zno = 0;
	dz_if_create_zinfo(dz.zinfo_nr_lines);

    }

    /* Update zone info */
    dz_if_refresh_zinfo();

    return;

}

static gboolean
dz_if_zstate_draw_legend_cb(GtkWidget *widget,
			    cairo_t *cr,
			    gpointer data)
{
    GtkAllocation allocation;
    cairo_text_extents_t te;
    GdkRGBA color;
    gint w, h, x = 10;

    /* Current size */
    gtk_widget_get_allocation(widget, &allocation);
    h = allocation.height;
    w = h / 2;

    /* Set font */
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10);
    
    /* Conventional zone legend */
    gdk_rgba_parse(&color, "Black");
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x, (h - w) / 2, w, w);
    cairo_stroke_preserve(cr);
    gdk_rgba_parse(&color, "Magenta");
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_fill(cr);
    x += w;

    cairo_text_extents(cr, "Conventional zone", &te);
    cairo_move_to(cr, x + 5 - te.x_bearing, h / 2 - te.height / 2 - te.y_bearing);
    cairo_show_text(cr, "Conventional zone");
    x += te.x_advance + 20;

    /* Seq unwritten zone legend */
    gdk_rgba_parse(&color, "Black");
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x, (h - w) / 2, w, w);
    cairo_stroke_preserve(cr);
    gdk_rgba_parse(&color, "Green");
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_fill(cr);
    x += w;

    cairo_text_extents(cr, "Sequential zone unwritten space", &te);
    cairo_move_to(cr, x + 5 - te.x_bearing, h / 2 - te.height / 2 - te.y_bearing);
    cairo_show_text(cr, "Sequential zone unwritten space");
    x += te.x_advance + 20;

    /* Seq written zone legend */
    gdk_rgba_parse(&color, "Black");
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x, (h - w) / 2, w, w);
    cairo_stroke_preserve(cr);
    gdk_rgba_parse(&color, "Red");
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_fill(cr);
    x += w;

    cairo_text_extents(cr, "Sequential zone written space", &te);
    cairo_move_to(cr, x + 5 - te.x_bearing, h / 2 - te.height / 2 - te.y_bearing);
    cairo_show_text(cr, "Sequential zone written space");

    return( FALSE );

}

#define DZ_DRAW_WOFST	5
#define DZ_DRAW_HOFST	20

static gboolean
dz_if_zstate_draw_cb(GtkWidget *widget,
		     cairo_t *cr,
		     gpointer data)
{
    GtkAllocation allocation;
    cairo_text_extents_t te;
    unsigned long long cap = 0, sz;
    struct zbc_zone *z;
    GdkRGBA color;
    int w, h, x = 0, zw, ww;
    char str[64];
    int i;

    /* Current size */
    gtk_widget_get_allocation(dz.zstate_da, &allocation);
    w = allocation.width;
    h = allocation.height;

    /* Get total viewed capacity */
    for(i = 0; i < dz.zinfo_nr_lines; i++) {
	z = &dz.zones[dz.zinfo_zno + i];
	cap += zbc_zone_length(z);
    }

    /* Draw zones */
    for(i = 0; i < dz.zinfo_nr_lines; i++) {

	z = &dz.zones[dz.zinfo_zno + i];

	/* Draw zone outline */
	zw = (w - DZ_DRAW_WOFST * 2) * zbc_zone_length(z) / cap;
	gdk_rgba_parse(&color, "Black");
	gdk_cairo_set_source_rgba(cr, &color);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, x + DZ_DRAW_WOFST, DZ_DRAW_HOFST, zw, h - DZ_DRAW_HOFST * 2);
	cairo_stroke_preserve(cr);

	if ( zbc_zone_conventional(z) ) {
	    gdk_rgba_parse(&color, "Magenta");
	} else if ( zbc_zone_full(z) ) {
	    gdk_rgba_parse(&color, "Red");
	} else {
	    gdk_rgba_parse(&color, "Green");
	}
	gdk_cairo_set_source_rgba(cr, &color);
	cairo_fill(cr);

	if ( (! zbc_zone_conventional(z))
	     && zbc_zone_open(z) ) {
	    /* Written space in zone */
	    ww = (zw * (zbc_zone_wp_lba(z) - zbc_zone_start_lba(z))) / zbc_zone_length(z);
	    if ( ww ) {
		gdk_rgba_parse(&color, "Red");
		gdk_cairo_set_source_rgba(cr, &color);
		cairo_rectangle(cr, x + DZ_DRAW_WOFST, DZ_DRAW_HOFST, ww, h - DZ_DRAW_HOFST * 2);
		cairo_fill(cr);
	    }
	}

	/* Set font */
	gdk_rgba_parse(&color, "Black");
	gdk_cairo_set_source_rgba(cr, &color);
	cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 10);

	/* Write zone number */
	sprintf(str, "%05d", dz.zinfo_zno + i);
	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr, x + DZ_DRAW_WOFST + zw / 2 - te.width / 2 - te.x_bearing, DZ_DRAW_HOFST - te.height / 2);
	cairo_show_text(cr, str);

	/* Write zone size */
	sz = zbc_zone_length(z) * dz.info.zbd_logical_block_size;
	if ( sz > (1024 * 1024 *1024) ) {
	    sprintf(str, "%llu GiB", sz / (1024 * 1024 *1024));
	} else {
	    sprintf(str, "%llu MiB", sz / (1024 * 1024));
	}
	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr, x + DZ_DRAW_WOFST + zw / 2 - te.width / 2 - te.x_bearing, h - te.height / 2);
	cairo_show_text(cr, str);

	x += zw;

    }

    return( FALSE );

}

static void
dz_if_delete_cb(GtkWidget *widget,
                GdkEvent *event,
                gpointer user_data)
{

    dz.window = NULL;

    gtk_main_quit();

    return;

}

static gboolean
dz_if_exit_cb(GtkWidget *widget,
              GdkEvent *event,
              gpointer user_data)
{

    gtk_main_quit();

    return( FALSE );

}

static gboolean
dz_if_timer_cb(gpointer user_data)
{

    dz_if_refresh_zones();

    return( TRUE );

}

static gboolean
dz_if_zinfo_update_cb(GtkWidget *widget,
		      GdkEvent *event,
		      gpointer user_data)
{
    GtkAllocation allocation;
    int nr_lines = dz.zinfo_nr_lines;

    /* Handle resize event */
    gtk_widget_get_allocation(dz.zinfo_viewport, &allocation);
    if ( dz.zinfo_height && dz.zinfo_line_height ) {
	
	if ( allocation.height > dz.zinfo_height ) {
	    /* Increase the number of lines displayed */
	    nr_lines += (allocation.height - dz.zinfo_height) / dz.zinfo_line_height;
	} else if ( allocation.height < dz.zinfo_height ) {
	    /* Decrease the number of lines displayed */
	    nr_lines -= (dz.zinfo_height - allocation.height) / dz.zinfo_line_height;
	    if ( nr_lines < DZ_ZONE_INFO_LINE_NUM ) {
		nr_lines = DZ_ZONE_INFO_LINE_NUM;
	    }
	}

	if ( nr_lines != dz.zinfo_nr_lines ) {
	    dz_if_create_zinfo(nr_lines);
	    dz.zinfo_height = allocation.height;
	}

    } else {

	dz.zinfo_height = allocation.height;

    }

    dz_if_refresh_zinfo();

    return( TRUE );
    
}

static gboolean
dz_if_zinfo_scroll_cb(GtkWidget *widget,
		      GdkEvent *event,
		      gpointer user_data)
{
    GdkEventScroll *scroll = &event->scroll;
    int zno = dz.zinfo_zno;

    if ( (event->type == (GdkEventType)GDK_SCROLL)
	 || (event->type == (GdkEventType)GDK_SCROLL_SMOOTH) ) {

	if ( scroll->direction == GDK_SCROLL_UP ) {
	    if ( zno > 0 ) {
		zno--;
	    }
	} else if ( scroll->direction == GDK_SCROLL_DOWN ) {
	    zno++;
	}

	if ( zno == dz.zinfo_zno ) {
	    goto out;
	}

	if ( zno > (int)(dz.nr_zones - dz.zinfo_nr_lines) ) {
	    zno = dz.nr_zones - dz.zinfo_nr_lines;
	}
	if ( zno < 0 ) {
	    zno = 0;
	}

	gtk_adjustment_configure(dz.zinfo_vadj,
				 zno,
				 0, dz.nr_zones,
				 1, dz.zinfo_nr_lines, dz.zinfo_nr_lines);

    }

out:

    return( TRUE );

}

static gboolean
dz_if_refresh_cb(GtkWidget *widget,
                 GdkEvent *event,
                 gpointer user_data)
{

    dz_if_refresh_zones();

    return( FALSE );

}

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data)
{
    GtkWidget *spinbutton = (GtkWidget *) user_data;
    GtkWidget *dialog;
    int zno, ret;

    gtk_spin_button_update(GTK_SPIN_BUTTON(spinbutton));
    zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));

    ret = dz_reset_zone(zno);
    if ( ret != 0 ) {

        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Reset zone write pointer failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Reset zone %d failed %d (%s)",
                                                 zno,
                                                 errno,
                                                 strerror(errno));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    dz_if_refresh_zones();

    return( FALSE );

}
