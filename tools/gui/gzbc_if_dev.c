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
dz_if_zinfo_print_zone_number(GtkTreeViewColumn *col,
			      GtkCellRenderer *renderer,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer user_data);

static void
dz_if_zinfo_print_zone_type(GtkTreeViewColumn *col,
			    GtkCellRenderer *renderer,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data);

static void
dz_if_zinfo_print_zone_cond(GtkTreeViewColumn *col,
			    GtkCellRenderer *renderer,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data);

static void
dz_if_zinfo_print_zone_need_reset(GtkTreeViewColumn *col,
				  GtkCellRenderer *renderer,
				  GtkTreeModel *model,
				  GtkTreeIter *iter,
				  gpointer user_data);

static void
dz_if_zinfo_print_zone_nonseq(GtkTreeViewColumn *col,
			      GtkCellRenderer *renderer,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer user_data);

static void
dz_if_zinfo_print_zone_start(GtkTreeViewColumn *col,
			     GtkCellRenderer *renderer,
			     GtkTreeModel *model,
			     GtkTreeIter *iter,
			     gpointer user_data);

static void
dz_if_zinfo_print_zone_length(GtkTreeViewColumn *col,
			      GtkCellRenderer *renderer,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer user_data);

static void
dz_if_zinfo_print_zone_wp(GtkTreeViewColumn *col,
			  GtkCellRenderer *renderer,
			  GtkTreeModel *model,
			  GtkTreeIter *iter,
			  gpointer user_data);

static gboolean
dz_if_zinfo_filter_cb(GtkComboBox *button,
                      gpointer user_data);

static gboolean
dz_if_refresh_cb(GtkWidget *widget,
		 gpointer user_data);

static void
dz_if_zinfo_fill(dz_dev_t *dzd);

static void
dz_if_zinfo_scroll_cb(GtkWidget *widget,
		      gpointer user_data);

static void
dz_if_zinfo_spinchanged_cb(GtkSpinButton *spin_button,
			   gpointer user_data);

static gboolean
dz_if_zinfo_select_cb(GtkTreeSelection *selection,
                      GtkTreeModel *model,
                      GtkTreePath *path,
                      gboolean path_currently_selected,
                      gpointer userdata);

static void
dz_if_update_zinfo(dz_dev_t *dzd);

static void
dz_if_redraw_zinfo(dz_dev_t *dzd);

static gboolean
dz_if_zstate_draw_legend_cb(GtkWidget *widget,
			    cairo_t *cr,
			    gpointer user_data);

static gboolean
dz_if_zstate_draw_cb(GtkWidget *widget,
		     cairo_t *cr,
		     gpointer user_data);

static void
dz_if_zinfo_update_range(dz_dev_t *dzd);

static gboolean
dz_if_open_cb(GtkWidget *widget,
               gpointer user_data);

static gboolean
dz_if_close_cb(GtkWidget *widget,
               gpointer user_data);

static gboolean
dz_if_finish_cb(GtkWidget *widget,
               gpointer user_data);

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               gpointer user_data);

static void
dz_if_set_block_size_cb(GtkWidget *widget,
			gpointer user_data);

static inline void
dz_if_set_margin(GtkWidget *widget,
                 int left, int right,
                 int top, int bottom)
{

#if GTK_CHECK_VERSION(3, 12, 0)
    gtk_widget_set_margin_start(widget, left);
    gtk_widget_set_margin_end(widget, right);
#else
    gtk_widget_set_margin_left(widget, left);
    gtk_widget_set_margin_right(widget, right);
#endif

    if ( top ) {
        gtk_widget_set_margin_top(widget, top);
    }

    if ( bottom ) {
        gtk_widget_set_margin_bottom(widget, bottom);
    }

    return;

}

static struct dz_if_zinfo_filter {
    int ro;
    char *str;
} zfilter[] =
    {
        { ZBC_RO_ALL,      "All zones"                      },
        { ZBC_RO_NOT_WP,   "Conventional zones"             },
        { ZBC_RO_EMPTY,    "Empty zones"                    },
        { ZBC_RO_FULL,     "Full zones"                     },
        { ZBC_RO_IMP_OPEN, "Implicitly open zones"          },
        { ZBC_RO_EXP_OPEN, "Explicitly open zones"          },
        { ZBC_RO_CLOSED,   "Closed zones"                   },
        { ZBC_RO_RESET,    "Zones needing reset"            },
        { ZBC_RO_NON_SEQ,  "Zones not sequentially written" },
        { ZBC_RO_RDONLY,   "Read-only zones"                },
        { ZBC_RO_OFFLINE,  "Offline zones"                  },
        { 0, NULL }
    };

/***** Definition of public functions *****/

dz_dev_t *
dz_if_dev_open(char *path)
{
    GtkWidget *top_vbox, *hbox, *ctrl_hbox, *vbox;
    GtkWidget *combo, *scrolledwindow;
    GtkWidget *frame;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *spinbutton;
    GtkWidget *hbuttonbox;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    GtkWidget *da;
    char str[256];
    unsigned int i;
    dz_dev_t *dzd;

    /* Open the device */
    dzd = dz_open(path);
    if ( ! dzd ) {
	return( NULL );
    }

    /* Create a top frame for the device */
    frame = gtk_frame_new(NULL);
    gtk_widget_show(frame);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
    dzd->page_frame = frame;

    /* Top vbox */
    top_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_show(top_vbox);
    gtk_container_add(GTK_CONTAINER(frame), top_vbox);

    /* Zone list filter frame */
    snprintf(str, sizeof(str) - 1,
             "<b>%.03F GB, %u B logical sectors, %u B physical sectors</b>",
             (double) (dzd->info.zbd_logical_blocks * dzd->info.zbd_logical_block_size) / 1000000000,
             dzd->info.zbd_logical_block_size,
             dzd->info.zbd_physical_block_size);
    frame = gtk_frame_new(str);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    dzd->zinfo_frame_label = gtk_frame_get_label_widget(GTK_FRAME(frame));
    gtk_label_set_use_markup(GTK_LABEL(dzd->zinfo_frame_label), TRUE);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_show(hbox);
    dz_if_set_margin(hbox, 7, 7, 0, 0);
    gtk_container_add(GTK_CONTAINER(frame), hbox);

    /* Zone list filter label */
    label = gtk_label_new(NULL);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_label_set_text(GTK_LABEL(label), "<b>Zone filter</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);

    /* Zone list filter */
    combo = gtk_combo_box_text_new();
    gtk_widget_show(combo);
    dz_if_set_margin(combo, 7, 7, 10, 5);
    i = 0;
    while( zfilter[i].str ) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, zfilter[i].str);
        i++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
    dzd->zfilter_combo = combo;

    g_signal_connect((gpointer) combo, "changed",
		     G_CALLBACK(dz_if_zinfo_filter_cb),
		     dzd);

    /* Refresh button */
    button = gtk_button_new();
    gtk_widget_show(button);
    dz_if_set_margin(button, 0, 7, 10, 5);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    image = gtk_image_new_from_icon_name("gtk-refresh", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new("Refresh");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "clicked", G_CALLBACK(dz_if_refresh_cb), dzd);

    /* Zone list frame */
    snprintf(str, sizeof(str) - 1, "<b>%d zones</b>", dzd->nr_zones);
    frame = gtk_frame_new(str);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(top_vbox), frame, TRUE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    dzd->zinfo_frame_label = gtk_frame_get_label_widget(GTK_FRAME(frame));
    gtk_label_set_use_markup(GTK_LABEL(dzd->zinfo_frame_label), TRUE);

    /* Create zone list */
    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    dz_if_set_margin(scrolledwindow, 7, 7, 10, 10);
    gtk_container_add(GTK_CONTAINER(frame), scrolledwindow);

    treeview = gtk_tree_view_new();
    gtk_widget_show(treeview);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), FALSE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)), GTK_SELECTION_SINGLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
    dzd->zinfo_treeview = treeview;

    g_signal_connect((gpointer) gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(treeview)), "value-changed",
		     G_CALLBACK(dz_if_zinfo_scroll_cb),
		     dzd);

    /* Number column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Zone number", renderer, "text", DZ_ZONE_NUM, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_number, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Type column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", DZ_ZONE_TYPE, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_type, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Condition column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Condition", renderer, "text", DZ_ZONE_COND, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_cond, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Need reset column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Need Reset", renderer, "text", DZ_ZONE_NEED_RESET, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_need_reset, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Non-seq column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Non Seq", renderer, "text", DZ_ZONE_NONSEQ, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_nonseq, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Start column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Start", renderer, "text", DZ_ZONE_START, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_start, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Length column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Length", renderer, "text", DZ_ZONE_LENGTH, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_length, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Write pointer column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Write Pointer", renderer, "text", DZ_ZONE_WP, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, dz_if_zinfo_print_zone_wp, dzd, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Create device list store */
    dzd->zinfo_store = gtk_list_store_new(DZ_ZONE_LIST_COLUMS,
					  G_TYPE_UINT,
					  G_TYPE_UINT,
					  G_TYPE_UINT,
					  G_TYPE_UINT,
					  G_TYPE_UINT,
					  G_TYPE_UINT64,
					  G_TYPE_UINT64,
					  G_TYPE_UINT64);
    for(i = 0; i < dzd->nr_zones; i++) {
        gtk_list_store_append(dzd->zinfo_store, &iter);
    }
    dzd->zinfo_model = GTK_TREE_MODEL(dzd->zinfo_store);
    gtk_tree_view_set_model(GTK_TREE_VIEW(dzd->zinfo_treeview), dzd->zinfo_model);
    g_object_unref(dzd->zinfo_model);

    dz_if_zinfo_fill(dzd);

    /* Zone use state drawing frame */
    frame = gtk_frame_new("<b>Zone State</b>");
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, TRUE, 0);
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
                     dzd);

    da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, -1, 100);
    gtk_widget_show(da);
    gtk_container_add(GTK_CONTAINER(vbox), da);
    dzd->zstate_da = da;

    g_signal_connect((gpointer) da, "draw",
                     G_CALLBACK(dz_if_zstate_draw_cb),
                     dzd);

    /* Controls frame */
    frame = gtk_frame_new(NULL);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    /* Hbox for controls */
    ctrl_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_show(ctrl_hbox);
    dz_if_set_margin(ctrl_hbox, 7, 7, 0, 0);
    gtk_container_add(GTK_CONTAINER(frame), ctrl_hbox);

    /* Reset zone control */
    label = gtk_label_new("<b>Zone number</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), label, FALSE, FALSE, 5);

    spinbutton = gtk_spin_button_new_with_range(-1, dzd->nr_zones - 1, 1);
    gtk_widget_show(spinbutton);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), TRUE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinbutton), 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), spinbutton, FALSE, FALSE, 5);
    dzd->zinfo_spinbutton = spinbutton;

    g_signal_connect((gpointer) spinbutton, "value-changed",
                     G_CALLBACK(dz_if_zinfo_spinchanged_cb),
                     dzd);

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

    label = gtk_label_new("Open Zone");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "clicked", G_CALLBACK(dz_if_open_cb), dzd);

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

    label = gtk_label_new("Close Zone");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "clicked", G_CALLBACK(dz_if_close_cb), dzd);

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

    label = gtk_label_new("Finish Zone");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "clicked", G_CALLBACK(dz_if_finish_cb), dzd);

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

    label = gtk_label_new("Reset Write Ptr");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "clicked", G_CALLBACK(dz_if_reset_cb), dzd);

    /* Block size */
    entry = gtk_entry_new();
    snprintf(str, sizeof(str) - 1, "%d", dzd->block_size);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    gtk_widget_show(entry);
    gtk_box_pack_end(GTK_BOX(ctrl_hbox), entry, FALSE, FALSE, 5);

    g_signal_connect((gpointer)entry, "activate", G_CALLBACK(dz_if_set_block_size_cb), dzd);

    label = gtk_label_new("<b>Block size (B)</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_widget_show(label);
    gtk_box_pack_end(GTK_BOX(ctrl_hbox), label, FALSE, FALSE, 5);

    /* Link tree view selection and spinbutton value */
    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
                                           dz_if_zinfo_select_cb,
                                           dzd,
                                           NULL);

    /* Done */
    gtk_widget_show_all(dz.window);
    dzd->zinfo_selection = -1;
    dz_if_zinfo_spinchanged_cb(GTK_SPIN_BUTTON(dzd->zinfo_spinbutton), dzd);

    return( dzd );

}

void
dz_if_dev_close(dz_dev_t *dzd)
{

    /* Close the device */
    dz_close(dzd);

    return;

}

void
dz_if_dev_update(dz_dev_t *dzd,
		 int do_report_zones)
{

    if ( do_report_zones ) {
	/* Update zinfo */
	dz_if_update_zinfo(dzd);
    } else {
	/* Update and redraw viewable zone range */
	dz_if_redraw_zinfo(dzd);
    }

    return;

}

/***** Definition of private functions *****/

static unsigned long long
dz_if_blocks(dz_dev_t *dzd,
	     unsigned long long lba)
{
    return( (lba * dzd->info.zbd_logical_block_size) / dzd->block_size );
}

static void
dz_if_zinfo_print_zone_number(GtkTreeViewColumn *col,
			      GtkCellRenderer *renderer,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer user_data)
{
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);

    /* Bold black font */
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
    g_object_set(renderer, "weight", PANGO_WEIGHT_BOLD, "weight-set", FALSE, NULL);
    snprintf(str, sizeof(str), "%d", i);
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_type(GtkTreeViewColumn *col,
			    GtkCellRenderer *renderer,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Normal black font */
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
    if ( zbc_zone_conventional(z) ) {
	strncpy(str, "Conventional", sizeof(str));
    } else if ( zbc_zone_sequential_req(z) ) {
	strncpy(str, "Seq write req.", sizeof(str));
    } else if ( zbc_zone_sequential_pref(z) ) {
	strncpy(str, "Seq write pref.", sizeof(str));
    } else {
	snprintf(str, sizeof(str), "??? (0x%01x)", zbc_zone_type(z));
    }
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_cond(GtkTreeViewColumn *col,
			    GtkCellRenderer *renderer,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Normal black font by default */
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
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
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_need_reset(GtkTreeViewColumn *col,
				  GtkCellRenderer *renderer,
				  GtkTreeModel *model,
				  GtkTreeIter *iter,
				  gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Zone need reset */
    if ( zbc_zone_need_reset(z) ) {
	g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	strncpy(str, "Yes", sizeof(str));
    } else {
	g_object_set(renderer, "foreground", "Green", "foreground-set", TRUE, NULL);
	strncpy(str, "No", sizeof(str));
    }
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_nonseq(GtkTreeViewColumn *col,
			      GtkCellRenderer *renderer,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Zone non seq */
    if ( zbc_zone_non_seq(z) ) {
	g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	strncpy(str, "Yes", sizeof(str));
    } else {
	g_object_set(renderer, "foreground", "Green", "foreground-set", TRUE, NULL);
	strncpy(str, "No", sizeof(str));
    }
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_start(GtkTreeViewColumn *col,
			     GtkCellRenderer *renderer,
			     GtkTreeModel *model,
			     GtkTreeIter *iter,
			     gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Zone start LBA */
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
    snprintf(str, sizeof(str), "%llu", dz_if_blocks(dzd, zbc_zone_start_lba(z)));
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_length(GtkTreeViewColumn *col,
			      GtkCellRenderer *renderer,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Zone length */
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
    snprintf(str, sizeof(str), "%llu", dz_if_blocks(dzd, zbc_zone_length(z)));
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_print_zone_wp(GtkTreeViewColumn *col,
			  GtkCellRenderer *renderer,
			  GtkTreeModel *model,
			  GtkTreeIter *iter,
			  gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    struct zbc_zone *z;
    char str[64];
    int i;

    gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);
    z = &dzd->zones[i];

    /* Zone wp LBA */
    if ( zbc_zone_not_wp(z) ) {
	g_object_set(renderer, "foreground", "Grey", "foreground-set", TRUE, NULL);
	strncpy(str, "N/A", sizeof(str));
    } else if ( zbc_zone_full(z) ) {
	g_object_set(renderer, "foreground", "Red", "foreground-set", TRUE, NULL);
	strncpy(str, "Full", sizeof(str));
    } else {
	g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
	snprintf(str, sizeof(str), "%llu", dz_if_blocks(dzd, zbc_zone_wp_lba(z)));
    }
    g_object_set(renderer, "text", str, NULL);

    return;

}

static void
dz_if_zinfo_fill(dz_dev_t *dzd)
{
    GtkTreeIter iter;
    struct zbc_zone *z;
    unsigned int i;

    /* Update device list */
    if ( (! dzd->nr_zones) || (! dzd->zones) ) {
        return;
    }

    gtk_tree_model_get_iter_first(dzd->zinfo_model, &iter);

    for(i = 0; i < dzd->nr_zones; i++) {

        z = &dzd->zones[i];

        gtk_list_store_set(dzd->zinfo_store, &iter,
                           DZ_ZONE_NUM, i,
                           DZ_ZONE_TYPE, z->zbz_type,
                           DZ_ZONE_COND, z->zbz_condition,
                           DZ_ZONE_NEED_RESET, zbc_zone_need_reset(z),
                           DZ_ZONE_NONSEQ, zbc_zone_non_seq(z),
                           DZ_ZONE_START, dz_if_blocks(dzd, zbc_zone_start_lba(z)),
                           DZ_ZONE_LENGTH, dz_if_blocks(dzd, zbc_zone_length(z)),
                           DZ_ZONE_WP, dz_if_blocks(dzd, zbc_zone_wp_lba(z)),
                           -1);

        gtk_tree_model_iter_next(dzd->zinfo_model, &iter);

    }

    return;

}

static void
dz_if_zinfo_update_range(dz_dev_t *dzd)
{
    GtkTreePath *start = NULL, *end = NULL;
    GtkTreeIter iter;

    if ( (! dzd->nr_zones) || (! dzd->zones) ) {
        dzd->zinfo_start_no = 0;
        dzd->zinfo_end_no = 0;
	return;
    }

    gtk_tree_view_get_visible_range(GTK_TREE_VIEW(dzd->zinfo_treeview), &start, &end);
    if ( start ) {
        if ( gtk_tree_model_get_iter(dzd->zinfo_model, &iter, start) == TRUE ) {
            gtk_tree_model_get(dzd->zinfo_model, &iter, DZ_ZONE_NUM, &dzd->zinfo_start_no, -1);
        } else {
            dzd->zinfo_start_no = 0;
        }
        gtk_tree_path_free(start);
    } else {
        dzd->zinfo_start_no = 0;
    }

    if ( end ) {
        if ( gtk_tree_model_get_iter(dzd->zinfo_model, &iter, end) == TRUE ) {
            gtk_tree_model_get(dzd->zinfo_model, &iter, DZ_ZONE_NUM, &dzd->zinfo_end_no, -1);
        } else {
            dzd->zinfo_end_no = dzd->nr_zones - 1;
        }
        gtk_tree_path_free(end);
    } else {
        dzd->zinfo_end_no = dzd->nr_zones - 1;
    }

    if ( dzd->zinfo_end_no >= dzd->nr_zones ) {
        dzd->zinfo_end_no = dzd->nr_zones - 1;
    }

    return;

}

static void
dz_if_redraw_zinfo(dz_dev_t *dzd)
{

    /* Re-draw zone state */
    gtk_widget_queue_draw(dzd->zstate_da);

    return;

}

static void
dz_if_zinfo_scroll_cb(GtkWidget *widget,
		      gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;

    /* Update viewd range in list */
    dz_if_redraw_zinfo(dzd);

    return;

}

static gboolean
dz_if_zinfo_select_cb(GtkTreeSelection *selection,
                      GtkTreeModel *model,
                      GtkTreePath *path,
                      gboolean path_currently_selected,
                      gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkWidget *spinbutton = dzd->zinfo_spinbutton;
    GtkTreeIter iter;
    int zno;

    if ( ! path_currently_selected ) {

        if ( ! dzd->nr_zones ) {

            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), 0.0);

        } else if ( gtk_tree_model_get_iter(model, &iter, path) ) {

            gtk_tree_model_get(model, &iter, DZ_ZONE_NUM, &dzd->zinfo_selection, -1);
            zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
            if ( zno != dzd->zinfo_selection ) {
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), (gdouble) dzd->zinfo_selection);
            }

        }

    }

    return TRUE;

}

static void
dz_if_zinfo_do_unselect(dz_dev_t *dzd)
{
    GtkTreeSelection *sel;
    GtkTreePath *path;

    if ( dzd->zinfo_selection >= 0 ) {

        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dzd->zinfo_treeview));
        path = gtk_tree_path_new_from_indices(dzd->zinfo_selection, -1);
        if ( path ) {
            gtk_tree_selection_unselect_path(sel, path);
            gtk_tree_path_free(path);
        }

        dzd->zinfo_selection = -1;

    }

    return;

}

static void
dz_if_zinfo_do_select(dz_dev_t *dzd,
		      int zno)
{
    GtkTreeSelection *sel;
    GtkTreePath *path;

    if ( zno != dzd->zinfo_selection ) {

        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dzd->zinfo_treeview));
        path = gtk_tree_path_new_from_indices(zno, -1);
        if ( path ) {
            gtk_tree_selection_select_path(sel, path);
            gtk_tree_path_free(path);
        }

        dzd->zinfo_selection = zno;

    }

    return;

}

static void
dz_if_zinfo_spinchanged_cb(GtkSpinButton *spinbutton,
			   gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    int zno = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));

    if ( (zno >= 0) && (zno < (gint)dzd->nr_zones) ) {
        dz_if_zinfo_do_select(dzd, zno);
	return;
    }

    dz_if_zinfo_do_unselect(dzd);

    return;

}

static void
dz_if_refresh_zinfo(dz_dev_t *dzd)
{
    unsigned int i;
    GtkTreeIter iter;
    char str[128];

    /* Update number of zones */
    snprintf(str, sizeof(str) - 1, "<b>%s: %d zones</b>", dzd->path, dzd->nr_zones);
    gtk_label_set_text(GTK_LABEL(dzd->zinfo_frame_label), str);
    gtk_label_set_use_markup(GTK_LABEL(dzd->zinfo_frame_label), TRUE);

    gtk_list_store_clear(dzd->zinfo_store);
    for(i = 0; i < dzd->nr_zones; i++) {
	gtk_list_store_append(dzd->zinfo_store, &iter);
    }

    /* Clear selection */
    dzd->zinfo_selection = -1;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(dzd->zinfo_spinbutton), -1, dzd->nr_zones);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dzd->zinfo_spinbutton), 0.0);
    gtk_spin_button_update(GTK_SPIN_BUTTON(dzd->zinfo_spinbutton));

    /* Update list */
    dz_if_zinfo_fill(dzd);

    /* Redraw visible range */
    dz_if_redraw_zinfo(dzd);

    return;

}

static void
dz_if_update_zinfo(dz_dev_t *dzd)
{
    GtkWidget *dialog;
    int ret;

    /* Update zone information */
    ret = dz_cmd_exec(dzd, DZ_CMD_REPORT_ZONES, 0, "Getting zone information...");
    if ( ret != 0 ) {

        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Get zone information failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Error %d (%s)",
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

	return;

    }

    /* Update list */
    dz_if_refresh_zinfo(dzd);

    return;

}

static gboolean
dz_if_zinfo_filter_cb(GtkComboBox *button,
                      gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkWidget *combo = dzd->zfilter_combo;
    int zone_ro = ZBC_RO_ALL;
    gchar *text;
    int i = 0;

    text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if ( text ) {

        while( zfilter[i].str ) {
            if ( strcmp(zfilter[i].str, text) == 0 ) {
                zone_ro = zfilter[i].ro;
                break;
            }
            i++;
        }

        g_free(text);

        if ( dzd->zone_ro != zone_ro ) {
            dzd->zone_ro = zone_ro;
            dz_if_update_zinfo(dzd);
        }

    }

    return( FALSE );

}

static gboolean
dz_if_refresh_cb(GtkWidget *widget,
		 gpointer user_data)
{

    dz_if_update_zinfo((dz_dev_t *) user_data);

    return( FALSE );

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
		     gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkAllocation allocation;
    cairo_text_extents_t te;
    unsigned long long cap = 0, sz;
    struct zbc_zone *z;
    GdkRGBA color;
    int w, h, x = 0, zw, ww;
    char str[64];
    unsigned int i;

    /* Current visible range */
    dz_if_zinfo_update_range(dzd);

    if ( (! dzd->zones) || (! dzd->nr_zones) ) {
        return( FALSE );
    }

    /* Current size */
    gtk_widget_get_allocation(dzd->zstate_da, &allocation);
    w = allocation.width - (DZ_DRAW_WOFST * 2);
    h = allocation.height;

    /* Get total viewed capacity */
    if ( dzd->zinfo_end_no >= dzd->nr_zones ) {
        dzd->zinfo_end_no = dzd->nr_zones - 1;
    }
    for(i = dzd->zinfo_start_no; i <= dzd->zinfo_end_no; i++) {
	cap += zbc_zone_length(&dzd->zones[i]);
    }

    /* Center overall drawing using x offset */
    zw = 0;
    for(i = dzd->zinfo_start_no; i <= dzd->zinfo_end_no; i++) {
	zw += ((unsigned long long)w * zbc_zone_length(&dzd->zones[i])) / cap;
    }
    x = DZ_DRAW_WOFST + (w - zw) / 2;

    /* Draw zones */
    for(i = dzd->zinfo_start_no; i <= dzd->zinfo_end_no; i++) {

	z = &dzd->zones[i];

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
	sz = zbc_zone_length(z) * dzd->info.zbd_logical_block_size;
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

static gboolean
dz_if_open_cb(GtkWidget *widget,
	      gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkWidget *spinbutton = dzd->zinfo_spinbutton;
    GtkWidget *dialog;
    int ret;

    dzd->zone_no = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
    ret = dz_cmd_exec(dzd, DZ_CMD_OPEN_ZONE, 1, NULL);
    if ( ret != 0 ) {

	if ( dzd->zone_no == -1 ) {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Open all zones failed\n");
	} else {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Open zone %d failed\n",
					    dzd->zone_no);
	}
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Error %d (%s)",
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_refresh_zinfo(dzd);

    return( FALSE );

}

static gboolean
dz_if_close_cb(GtkWidget *widget,
               gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkWidget *spinbutton = dzd->zinfo_spinbutton;
    GtkWidget *dialog;
    int ret;

    dzd->zone_no = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
    ret = dz_cmd_exec(dzd, DZ_CMD_CLOSE_ZONE, 1, NULL);
    if ( ret != 0 ) {

	if ( dzd->zone_no == -1 ) {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Close all zones failed\n");
	} else {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Close zone %d failed\n",
					    dzd->zone_no);
	}
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Error %d (%s)",
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_refresh_zinfo(dzd);

    return( FALSE );

}

static gboolean
dz_if_finish_cb(GtkWidget *widget,
		gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkWidget *spinbutton = dzd->zinfo_spinbutton;
    GtkWidget *dialog;
    int ret;

    dzd->zone_no = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
    ret = dz_cmd_exec(dzd, DZ_CMD_FINISH_ZONE, 1, NULL);
    if ( ret != 0 ) {

        if ( dzd->zone_no == -1 ) {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Finish all zones failed\n");
	} else {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Finish zone %d failed\n",
					    dzd->zone_no);
	}
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Error %d (%s)",
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_refresh_zinfo(dzd);

    return( FALSE );

}

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    GtkWidget *spinbutton = dzd->zinfo_spinbutton;
    GtkWidget *dialog;
    char msg[128];
    int ret;

    dzd->zone_no = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
    if ( dzd->zone_no == -1 ) {
	strcpy(msg, "Resetting zones...");
    } else {
	sprintf(msg, "Resetting zone %d...", dzd->zone_no);
    }
    ret = dz_cmd_exec(dzd, DZ_CMD_RESET_ZONE, 1, msg);
    if ( ret != 0 ) {

        if ( dzd->zone_no == -1 ) {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Reset all zones write pointer failed\n");
	} else {
	    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR,
					    GTK_BUTTONS_OK,
					    "Reset zone %d write pointer failed\n",
					    dzd->zone_no);
	}
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Error %d (%s)",
                                                 ret,
                                                 strerror(ret));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

    }

    /* Update zinfo */
    dz_if_refresh_zinfo(dzd);

    return( FALSE );

}

static void
dz_if_set_block_size_cb(GtkWidget *widget,
			gpointer user_data)
{
    dz_dev_t *dzd = (dz_dev_t *) user_data;
    char *val_str = (char *) gtk_entry_get_text(GTK_ENTRY(widget));

    if ( val_str && dzd ) {

	int block_size = atoi(val_str);

	if ( block_size > 0 ) {
	    dzd->block_size = block_size;
	} else {
	    char str[32];
	    if ( dz.block_size ) {
		dzd->block_size = dz.block_size;
	    } else {
		dzd->block_size = dzd->info.zbd_logical_block_size;
	    }
	    snprintf(str, sizeof(str) - 1, "%d", dzd->block_size);
	    gtk_entry_set_text(GTK_ENTRY(widget), str);
	}
	dz_if_update_zinfo(dzd);

    }

    return;

}

