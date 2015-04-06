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
#include <signal.h>
#include <fcntl.h>

#include "gzbc.h"

/***** Declaration of private functions *****/

static void
dz_if_zinfo_print(GtkTreeViewColumn *col,
                  GtkCellRenderer *renderer,
                  GtkTreeModel *model,
                  GtkTreeIter *iter,
                  gpointer user_data);

static void
dz_if_zinfo_fill(void);

static gboolean
dz_if_zinfo_scroll_cb(GtkWidget *widget,
		      GdkEvent *event,
		      gpointer user_data);

static gboolean
dz_if_zinfo_select_cb(GtkTreeSelection *selection,
                      GtkTreeModel *model,
                      GtkTreePath *path,
                      gboolean path_currently_selected,
                      gpointer userdata);

static gboolean
dz_if_resize_cb(GtkWidget *widget,
                GdkEvent *event,
                gpointer user_data);

static gboolean
dz_if_zstate_draw_legend_cb(GtkWidget *widget,
			    cairo_t *cr,
			    gpointer user_data);

static gboolean
dz_if_zstate_draw_cb(GtkWidget *widget,
		     cairo_t *cr,
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
dz_if_open_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data);

static gboolean
dz_if_close_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data);

static gboolean
dz_if_finish_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data);

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data);

/***** Definition of public functions *****/

int
dz_if_create(void)
{
    GtkWidget *win_vbox, *hbox, *ctrl_hbox, *vbox;
    GtkWidget *scrolledwindow;
    GtkWidget *window;
    GtkWidget *frame;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *spinbutton;
    GtkWidget *hbuttonbox;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    GtkWidget *da;
    char str[128];
    unsigned int i;

    /* Get colors */
    gdk_rgba_parse(&dz.conv_color, "Magenta");
    gdk_rgba_parse(&dz.seqnw_color, "Green");
    gdk_rgba_parse(&dz.seqw_color, "Red");

    /* Window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ZBC Device Zone State");
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_resize(GTK_WINDOW(window), 720, 640);
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
    dz.zinfo_frame_label = gtk_frame_get_label_widget(GTK_FRAME(frame));
    gtk_label_set_use_markup(GTK_LABEL(dz.zinfo_frame_label), TRUE);

    /* Create zone list */
    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
#if GTK_CHECK_VERSION(3, 12, 0)
    gtk_widget_set_margin_start(scrolledwindow, 7);
    gtk_widget_set_margin_end(scrolledwindow, 7);
#else
    gtk_widget_set_margin_left(scrolledwindow, 7);
    gtk_widget_set_margin_right(scrolledwindow, 7);
#endif
    gtk_widget_set_margin_top(scrolledwindow, 10);
    gtk_widget_set_margin_bottom(scrolledwindow, 10);
    gtk_container_add(GTK_CONTAINER(frame), scrolledwindow);

    treeview = gtk_tree_view_new();
    gtk_widget_show(treeview);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), FALSE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)), GTK_SELECTION_SINGLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
    dz.zinfo_treeview = treeview;

    g_signal_connect((gpointer) gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(treeview)), "value-changed",
		     G_CALLBACK(dz_if_zinfo_scroll_cb),
		     NULL);

    /* Number column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Zone number", renderer, "text", DZ_ZONE_NUM, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_NUM, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Type column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", DZ_ZONE_TYPE, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_TYPE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Condition column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Condition", renderer, "text", DZ_ZONE_COND, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_COND, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Need reset column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Need Reset", renderer, "text", DZ_ZONE_NEED_RESET, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_NEED_RESET, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Non-seq column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Non Seq", renderer, "text", DZ_ZONE_NONSEQ, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_NONSEQ, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Start column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Start", renderer, "text", DZ_ZONE_START, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_START, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Length column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Length", renderer, "text", DZ_ZONE_LENGTH, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_LENGTH, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Write pointer column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Write Pointer", renderer, "text", DZ_ZONE_WP, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print, (gpointer) DZ_ZONE_WP, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Create device list store */
    dz.zinfo_store = gtk_list_store_new(DZ_ZONE_LIST_COLUMS,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT64,
                                        G_TYPE_UINT64,
                                        G_TYPE_UINT64);
    for(i = 0; i < dz.nr_zones; i++) {
        gtk_list_store_append(dz.zinfo_store, &iter);
    }
    dz.zinfo_model = GTK_TREE_MODEL(dz.zinfo_store);
    gtk_tree_view_set_model(GTK_TREE_VIEW(dz.zinfo_treeview), dz.zinfo_model);
    g_object_unref(dz.zinfo_model);

    dz_if_zinfo_fill();

    /* Zone use state drawing frame */
    frame = gtk_frame_new("<b>Zone State</b>");
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(win_vbox), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

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

    /* Hbox for controls */
    ctrl_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(ctrl_hbox);
#if GTK_CHECK_VERSION(3, 12, 0)
    gtk_widget_set_margin_start(ctrl_hbox, 7);
    gtk_widget_set_margin_end(ctrl_hbox, 7);
#else
    gtk_widget_set_margin_left(ctrl_hbox, 7);
    gtk_widget_set_margin_right(ctrl_hbox, 7);
#endif
    gtk_container_add(GTK_CONTAINER(frame), ctrl_hbox);

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

    /* Open button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    image = gtk_image_new_from_icon_name("gtk-connect", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Open Zone");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_open_cb),
                      spinbutton);

    /* Close button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    image = gtk_image_new_from_icon_name("gtk-close", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Close Zone");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_close_cb),
                      spinbutton);

    /* Finish button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    image = gtk_image_new_from_icon_name("gtk-goto-last", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Finish Zone");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_finish_cb),
                      spinbutton);

    /* Reset button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);

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
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);
    
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

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    image = gtk_image_new_from_icon_name("gtk-quit", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Exit");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_exit_cb),
                      &dz);

    if ( dz.interval >= DZ_INTERVAL ) {
        /* Add timer for automatic refresh */
        dz.timer_id = g_timeout_add(dz.interval, dz_if_timer_cb, NULL);
    }

    /* Link tree view selection and spinbutton value */
    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
                                           dz_if_zinfo_select_cb,
                                           spinbutton,
                                           NULL);

    g_signal_connect((gpointer) dz.window, "configure-event",
		     G_CALLBACK(dz_if_resize_cb),
		     NULL);

    /* Done */
    gtk_widget_show_all(dz.window);

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

static unsigned long long
dz_if_blocks(unsigned long long lba)
{
    return( (lba * dz.info.zbd_logical_block_size) / dz.block_size );
}

static void
dz_if_zinfo_print(GtkTreeViewColumn *col,
                  GtkCellRenderer *renderer,
                  GtkTreeModel *model,
                  GtkTreeIter *iter,
                  gpointer user_data)
{
    long c = (long) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    /* Normal black font by default */
    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dz.zones[i];
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);

    switch( c ) {

    case DZ_ZONE_NUM:

        /* ZOne number */
        g_object_set(renderer, "weight", PANGO_WEIGHT_BOLD, "weight-set", FALSE, NULL);
        snprintf(str, sizeof(str), "%d", i);
        break;

    case DZ_ZONE_TYPE:

        /* Zone type */
	if ( zbc_zone_conventional(z) ) {
	    strncpy(str, "Conventional", sizeof(str));
	} else if ( zbc_zone_sequential_req(z) ) {
	    strncpy(str, "Seq write req.", sizeof(str));
	} else if ( zbc_zone_sequential_pref(z) ) {
	    strncpy(str, "Seq write pref.", sizeof(str));
	} else {
	    snprintf(str, sizeof(str), "??? (0x%01x)", zbc_zone_type(z));
	}
        break;

    case DZ_ZONE_COND:

        /* Zone condition */
	if ( zbc_zone_not_wp(z) ) {
	    strncpy(str, "Not WP", sizeof(str));
	} else if ( zbc_zone_empty(z) ) {
            g_object_set(renderer, "foreground", "Green", "foreground-set", TRUE, NULL);
	    strncpy(str, "Empty", sizeof(str));
	} else if ( zbc_zone_full(z) ) {
            g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	    strncpy(str, "Full", sizeof(str));
        } else if ( zbc_zone_imp_open(z) ) {
            g_object_set(renderer, "foreground", "Blue", "foreground-set", TRUE, NULL);
            strncpy(str, "Implicit Open", sizeof(str));
        } else if ( zbc_zone_exp_open(z) ) {
            g_object_set(renderer, "foreground", "Blue", "foreground-set", TRUE, NULL);
            strncpy(str, "Explicit Open", sizeof(str));
        } else if ( zbc_zone_closed(z) ) {
            strncpy(str, "Closed", sizeof(str));
	} else if ( zbc_zone_rdonly(z) ) {
	    strncpy(str, "Read-only", sizeof(str));
	} else if ( zbc_zone_offline(z) ) {
	    strncpy(str, "Offline", sizeof(str));
	} else {
	    snprintf(str, sizeof(str), "??? (0x%01x)", z->zbz_condition);
	}
        break;

    case DZ_ZONE_NEED_RESET:

        /* Zone need reset */
        if ( zbc_zone_need_reset(z) ) {
            g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	    strncpy(str, "Yes", sizeof(str));
	} else {
            g_object_set(renderer, "foreground", "Green", "foreground-set", TRUE, NULL);
	    strncpy(str, "No", sizeof(str));
        }
        break;

    case DZ_ZONE_NONSEQ:

        /* Zone non seq */
	if ( zbc_zone_non_seq(z) ) {
            g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	    strncpy(str, "Yes", sizeof(str));
	} else {
            g_object_set(renderer, "foreground", "Green", "foreground-set", TRUE, NULL);
	    strncpy(str, "No", sizeof(str));
	}
        break;

    case DZ_ZONE_START:

        /* Zone start LBA */
        snprintf(str, sizeof(str), "%llu", dz_if_blocks(zbc_zone_start_lba(z)));
        break;

    case DZ_ZONE_LENGTH:

	/* Zone length */
        snprintf(str, sizeof(str), "%llu", dz_if_blocks(zbc_zone_length(z)));
        break;

    case DZ_ZONE_WP:

	/* Zone wp LBA */
	if ( zbc_zone_not_wp(z) ) {
            g_object_set(renderer, "foreground", "Grey", "foreground-set", TRUE, NULL);
	    strncpy(str, "N/A", sizeof(str));
	} else if ( zbc_zone_full(z) ) {
            g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	    strncpy(str, "Full", sizeof(str));
	} else {
	    snprintf(str, sizeof(str), "%llu", dz_if_blocks(zbc_zone_wp_lba(z)));
	}
        break;

    }
        
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_fill(void)
{
    GtkTreeIter iter;
    struct zbc_zone *z;
    unsigned int i;

    /* Update device list */
    gtk_tree_model_get_iter_first(dz.zinfo_model, &iter);

    for(i = 0; i < dz.nr_zones; i++) {

        z = &dz.zones[i];

        gtk_list_store_set(dz.zinfo_store, &iter,
                           DZ_ZONE_NUM, i,
                           DZ_ZONE_TYPE, z->zbz_type,
                           DZ_ZONE_COND, z->zbz_condition,
                           DZ_ZONE_NEED_RESET, zbc_zone_need_reset(z),
                           DZ_ZONE_NONSEQ, zbc_zone_non_seq(z),
                           DZ_ZONE_START, dz_if_blocks(zbc_zone_start_lba(z)),
                           DZ_ZONE_LENGTH, dz_if_blocks(zbc_zone_length(z)),
                           DZ_ZONE_WP, dz_if_blocks(zbc_zone_wp_lba(z)),
                           -1);

        gtk_tree_model_iter_next(dz.zinfo_model, &iter);

    }

    //gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dz.zinfo_treeview));

    return;

}

static void
dz_if_zinfo_update_range(void)
{
    GtkTreePath *start = NULL, *end = NULL;
    GtkTreeIter iter;

    gtk_tree_view_get_visible_range(GTK_TREE_VIEW(dz.zinfo_treeview), &start, &end);

    if ( start ) {

        if ( gtk_tree_model_get_iter(dz.zinfo_model, &iter, start) == TRUE ) {
            gtk_tree_model_get(dz.zinfo_model, &iter, 
                               DZ_ZONE_NUM, &dz.zinfo_start_no,
                               -1);
        } else {
            dz.zinfo_start_no = 0;
        }

        gtk_tree_path_free(start);

    } else {

        dz.zinfo_start_no = 0;

    }
    
    if ( end ) {

        if ( gtk_tree_model_get_iter(dz.zinfo_model, &iter, end) == TRUE ) {
            gtk_tree_model_get(dz.zinfo_model, &iter, 
                               DZ_ZONE_NUM, &dz.zinfo_end_no,
                               -1);
        } else {
            dz.zinfo_end_no = dz.nr_zones - 1;
        }

        gtk_tree_path_free(end);

    } else {

        dz.zinfo_end_no = dz.nr_zones - 1;

    }

    if ( dz.zinfo_end_no >= dz.nr_zones ) {
        dz.zinfo_end_no = dz.nr_zones - 1;
    }

    return;

}

static void
dz_if_redraw_zinfo(void)
{

    /* Re-draw zone state */
    gtk_widget_queue_draw(dz.zstate_da);

    return;

}

static gboolean
dz_if_zinfo_scroll_cb(GtkWidget *widget,
		      GdkEvent *event,
		      gpointer user_data)
{

    /* Update viewd range in list */
    dz_if_zinfo_update_range();

    dz_if_redraw_zinfo();

    return( TRUE );

}

static gboolean
dz_if_zinfo_select_cb(GtkTreeSelection *selection,
                      GtkTreeModel *model,
                      GtkTreePath *path,
                      gboolean path_currently_selected,
                      gpointer user_data)
{
    GtkWidget *spinbutton = (GtkWidget *) user_data;
    GtkTreeIter iter;
    int i, zno;

    if ( ! path_currently_selected ) {

        if ( gtk_tree_model_get_iter(model, &iter, path) ) {
            gtk_tree_model_get(model, &iter, DZ_ZONE_NUM, &i, -1);
            gtk_spin_button_update(GTK_SPIN_BUTTON(spinbutton));
            zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
            if ( zno != i ) {
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), (gdouble) i);
            }
        }

    }

    return TRUE;

}

static gboolean
dz_if_resize_cb(GtkWidget *widget,
                GdkEvent *event,
                gpointer user_data)
{

    /* Update viewd range in list */
    dz_if_zinfo_update_range();

    dz.zinfo_lines = dz.zinfo_end_no + 1 - dz.zinfo_start_no;
    dz.zinfo_lines = (dz.zinfo_lines / 2) * 2;

    dz_if_redraw_zinfo();

    return( FALSE );

}

static void
dz_if_update_zinfo(void)
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
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

	return;

    }

    if ( dz.nr_zones != nr_zones ) {
	/* Number of zones changed... */
	snprintf(str, sizeof(str) - 1, "<b>%s: %d zones</b>", dz.path, dz.nr_zones);
	gtk_label_set_text(GTK_LABEL(dz.zinfo_frame_label), str);
	gtk_label_set_use_markup(GTK_LABEL(dz.zinfo_frame_label), TRUE);
    }

    /* Update list */
    dz_if_zinfo_fill();

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
    gdk_cairo_set_source_rgba(cr, &dz.conv_color);
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
    gdk_cairo_set_source_rgba(cr, &dz.seqnw_color);
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
    gdk_cairo_set_source_rgba(cr, &dz.seqw_color);
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
    unsigned int i;

    /* Current size */
    gtk_widget_get_allocation(dz.zstate_da, &allocation);
    w = allocation.width - (DZ_DRAW_WOFST * 2);
    h = allocation.height;

    /* Get total viewed capacity */
    dz.zinfo_end_no = dz.zinfo_start_no + dz.zinfo_lines - 1;
    if ( dz.zinfo_end_no >= dz.nr_zones ) {
        dz.zinfo_end_no = dz.nr_zones - 1;
    }
    for(i = dz.zinfo_start_no; i <= dz.zinfo_end_no; i++) {
	cap += zbc_zone_length(&dz.zones[i]);
    }

    /* Center overall drawing using x offset */
    zw = 0;
    for(i = dz.zinfo_start_no; i <= dz.zinfo_end_no; i++) {
	zw += ((unsigned long long)w * zbc_zone_length(&dz.zones[i])) / cap;
    }
    x = DZ_DRAW_WOFST + (w - zw) / 2;

    /* Draw zones */
    for(i = dz.zinfo_start_no; i <= dz.zinfo_end_no; i++) {

	z = &dz.zones[i];

	/* Draw zone outline */
	zw = (w * zbc_zone_length(z)) / cap;
	gdk_rgba_parse(&color, "Black");
	gdk_cairo_set_source_rgba(cr, &color);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, x, DZ_DRAW_HOFST, zw, h - (DZ_DRAW_HOFST * 2));
	cairo_stroke_preserve(cr);

	if ( zbc_zone_conventional(z) ) {
            gdk_cairo_set_source_rgba(cr, &dz.conv_color);
	} else if ( zbc_zone_full(z) ) {
            gdk_cairo_set_source_rgba(cr, &dz.seqw_color);
	} else {
            gdk_cairo_set_source_rgba(cr, &dz.seqnw_color);
	}
	cairo_fill(cr);

	if ( (! zbc_zone_conventional(z))
             && (zbc_zone_imp_open(z) 
                 || zbc_zone_exp_open(z) 
                 || zbc_zone_closed(z)) ) {
	    /* Written space in zone */
	    ww = (zw * (zbc_zone_wp_lba(z) - zbc_zone_start_lba(z))) / zbc_zone_length(z);
	    if ( ww ) {
		gdk_cairo_set_source_rgba(cr, &dz.seqw_color);
		cairo_rectangle(cr, x, DZ_DRAW_HOFST, ww, h - DZ_DRAW_HOFST * 2);
		cairo_fill(cr);
	    }
	}

	/* Set font */
	gdk_rgba_parse(&color, "Black");
	gdk_cairo_set_source_rgba(cr, &color);
	cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 10);

	/* Write zone number */
	sprintf(str, "%05d", i);
	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr, x + zw / 2 - te.width / 2 - te.x_bearing, DZ_DRAW_HOFST - te.height / 2);
	cairo_show_text(cr, str);

	/* Write zone size */
	sz = zbc_zone_length(z) * dz.info.zbd_logical_block_size;
	if ( sz > (1024 * 1024 *1024) ) {
	    sprintf(str, "%llu GiB", sz / (1024 * 1024 *1024));
	} else {
	    sprintf(str, "%llu MiB", sz / (1024 * 1024));
	}
	cairo_set_font_size(cr, 8);
	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr, x + zw / 2 - te.width / 2 - te.x_bearing, h - te.height / 2);
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

    /* Update zinfo */
    dz_if_update_zinfo();

    /* Redraw zone info */
    dz_if_redraw_zinfo();

    return( TRUE );

}

static gboolean
dz_if_refresh_cb(GtkWidget *widget,
                 GdkEvent *event,
                 gpointer user_data)
{

    /* Update zinfo */
    dz_if_update_zinfo();

    /* Redraw zone info */
    dz_if_redraw_zinfo();

    return( FALSE );

}

static gboolean
dz_if_open_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data)
{
    GtkWidget *spinbutton = (GtkWidget *) user_data;
    GtkWidget *dialog;
    int zno, ret;

    gtk_spin_button_update(GTK_SPIN_BUTTON(spinbutton));
    zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));

    ret = dz_open_zone(zno);
    if ( ret != 0 ) {

        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Open zone failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Open zone %d failed %d (%s)",
                                                 zno,
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_update_zinfo();

    /* Redraw zone info */
    dz_if_redraw_zinfo();

    return( FALSE );

}

static gboolean
dz_if_close_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data)
{
    GtkWidget *spinbutton = (GtkWidget *) user_data;
    GtkWidget *dialog;
    int zno, ret;

    gtk_spin_button_update(GTK_SPIN_BUTTON(spinbutton));
    zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));

    ret = dz_close_zone(zno);
    if ( ret != 0 ) {

        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Close zone failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Close zone %d failed %d (%s)",
                                                 zno,
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_update_zinfo();

    /* Redraw zone info */
    dz_if_redraw_zinfo();

    return( FALSE );

}

static gboolean
dz_if_finish_cb(GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data)
{
    GtkWidget *spinbutton = (GtkWidget *) user_data;
    GtkWidget *dialog;
    int zno, ret;

    gtk_spin_button_update(GTK_SPIN_BUTTON(spinbutton));
    zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));

    ret = dz_finish_zone(zno);
    if ( ret != 0 ) {

        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Finish zone failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Finish zone %d failed %d (%s)",
                                                 zno,
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_update_zinfo();

    /* Redraw zone info */
    dz_if_redraw_zinfo();

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
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_update_zinfo();

    /* Redraw zone info */
    dz_if_redraw_zinfo();

    return( FALSE );

}

