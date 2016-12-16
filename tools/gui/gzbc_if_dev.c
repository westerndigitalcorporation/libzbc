/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public License version 3, "as is," without technical
 * support, and WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
 * received a copy of the GNU Lesser General Public License along with libzbc.
 * If not, see <http://www.gnu.org/licenses/>.
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

static void dz_if_zlist_print_zone_number(GtkTreeViewColumn *col,
					  GtkCellRenderer *renderer,
					  GtkTreeModel *model,
					  GtkTreeIter *iter,
					  gpointer user_data);

static void dz_if_zlist_print_zone_type(GtkTreeViewColumn *col,
					GtkCellRenderer *renderer,
					GtkTreeModel *model,
					GtkTreeIter *iter,
					gpointer user_data);

static void dz_if_zlist_print_zone_cond(GtkTreeViewColumn *col,
					GtkCellRenderer *renderer,
					GtkTreeModel *model,
					GtkTreeIter *iter,
					gpointer user_data);

static void dz_if_zlist_print_zone_rwp_recommended(GtkTreeViewColumn *col,
						   GtkCellRenderer *renderer,
						   GtkTreeModel *model,
						   GtkTreeIter *iter,
						   gpointer user_data);

static void dz_if_zlist_print_zone_nonseq(GtkTreeViewColumn *col,
					  GtkCellRenderer *renderer,
					  GtkTreeModel *model,
					  GtkTreeIter *iter,
					  gpointer user_data);

static void dz_if_zlist_print_zone_start(GtkTreeViewColumn *col,
					 GtkCellRenderer *renderer,
					 GtkTreeModel *model,
					 GtkTreeIter *iter,
					 gpointer user_data);

static void dz_if_zlist_print_zone_length(GtkTreeViewColumn *col,
					  GtkCellRenderer *renderer,
					  GtkTreeModel *model,
					  GtkTreeIter *iter,
					  gpointer user_data);

static void dz_if_zlist_print_zone_wp(GtkTreeViewColumn *col,
				      GtkCellRenderer *renderer,
				      GtkTreeModel *model,
				      GtkTreeIter *iter,
				      gpointer user_data);

static gboolean dz_if_zlist_filter_cb(GtkComboBox *button, gpointer user_data);
static void dz_if_zlist_set_block_size_cb(GtkEntry *entry, gpointer user_data);
static void dz_if_zlist_set_use_hexa_cb(GtkToggleButton *togglebutton,
					gpointer user_data);
static gboolean dz_if_zlist_entry_visible(GtkTreeModel *model,
					  GtkTreeIter *iter, gpointer data);
static void dz_if_zlist_fill(dz_dev_t *dzd);
static gboolean dz_if_zlist_refresh_cb(GtkWidget *widget, gpointer user_data);
static void dz_if_zlist_scroll_cb(GtkWidget *widget, gpointer user_data);
static gboolean dz_if_zlist_select_cb(GtkTreeSelection *selection,
				      GtkTreeModel *model, GtkTreePath *path,
				      gboolean path_currently_selected,
				      gpointer userdata);

static void dz_if_znum_set(dz_dev_t *dzd);
static void dz_if_znum_set_cb(GtkEntry *entry, gpointer user_data);

static void dz_if_zblock_set(dz_dev_t *dzd);
static void dz_if_zblock_set_cb(GtkEntry *entry, gpointer user_data);

static void dz_if_update_zones(dz_dev_t *dzd);
static void dz_if_redraw_zones(dz_dev_t *dzd);
static gboolean dz_if_zones_draw_legend_cb(GtkWidget *widget, cairo_t *cr,
					   gpointer user_data);
static gboolean dz_if_zones_draw_cb(GtkWidget *widget, cairo_t *cr,
				    gpointer user_data);

static gboolean dz_if_zone_open_cb(GtkWidget *widget, gpointer user_data);
static gboolean dz_if_zone_close_cb(GtkWidget *widget, gpointer user_data);
static gboolean dz_if_zone_finish_cb(GtkWidget *widget, gpointer user_data);
static gboolean dz_if_zone_reset_cb(GtkWidget *widget, gpointer user_data);

static inline void dz_if_set_margin(GtkWidget *widget,
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

	if (top)
		gtk_widget_set_margin_top(widget, top);

	if (bottom)
		gtk_widget_set_margin_bottom(widget, bottom);
}

static struct dz_if_zinfo_filter {
	int ro;
	char *str;
} zfilter[] = {
        { ZBC_RO_ALL,			"All zones"                      },
        { ZBC_RO_NOT_WP,		"Conventional zones"             },
        { ZBC_RO_EMPTY,			"Empty zones"                    },
        { ZBC_RO_FULL,			"Full zones"                     },
        { ZBC_RO_IMP_OPEN,		"Implicitly open zones"          },
        { ZBC_RO_EXP_OPEN,		"Explicitly open zones"          },
        { ZBC_RO_CLOSED,		"Closed zones"                   },
        { ZBC_RO_RWP_RECOMMENDED,	"Zones needing reset"            },
        { ZBC_RO_NON_SEQ,		"Zones not sequentially written" },
        { ZBC_RO_RDONLY,		"Read-only zones"                },
        { ZBC_RO_OFFLINE,		"Offline zones"			 },
        { 0, NULL }
};

dz_dev_t * dz_if_dev_open(char *path)
{
	GtkWidget *top_vbox, *hbox, *ctrl_hbox, *vbox;
	GtkWidget *combo, *scrolledwindow;
	GtkWidget *button, *checkbutton;
	GtkWidget *frame;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *entry;
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
	if (!dzd)
		return NULL;

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
		 "<b>%.03F GB, %u B logical blocks, %u B physical blocks</b>",
		 (double) (dzd->info.zbd_sectors << 9) / 1000000000,
		 dzd->info.zbd_lblock_size,
		 dzd->info.zbd_pblock_size);
	frame = gtk_frame_new(str);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, TRUE, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);
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
	while (zfilter[i].str) {
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, zfilter[i].str);
		i++;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
	dzd->zfilter_combo = combo;

	g_signal_connect((gpointer) combo,
			 "changed",
			 G_CALLBACK(dz_if_zlist_filter_cb),
			 dzd);

	/* Block size entry */
	label = gtk_label_new("<b>Block size (B)</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	snprintf(str, sizeof(str) - 1, "%d", dzd->block_size);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);

	g_signal_connect((gpointer)entry,
			 "activate",
			 G_CALLBACK(dz_if_zlist_set_block_size_cb),
			 dzd);

	/* Hexadecimal values */
	label = gtk_label_new("<b>Hexadecimal</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	checkbutton = gtk_check_button_new();
	gtk_widget_show(checkbutton);
	gtk_box_pack_start(GTK_BOX(hbox), checkbutton, FALSE, FALSE, 0);

	g_signal_connect((gpointer)checkbutton, "toggled",
			 G_CALLBACK(dz_if_zlist_set_use_hexa_cb),
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

	g_signal_connect((gpointer) button,
			 "clicked",
			 G_CALLBACK(dz_if_zlist_refresh_cb),
			 dzd);

	/* Zone list frame */
	snprintf(str, sizeof(str) - 1,
		 "<b>%s: %d zones</b>",
		 dzd->path, dzd->nr_zones);
	frame = gtk_frame_new(str);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(top_vbox), frame, TRUE, TRUE, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	dzd->zlist_frame_label = gtk_frame_get_label_widget(GTK_FRAME(frame));
	gtk_label_set_use_markup(GTK_LABEL(dzd->zlist_frame_label), TRUE);

	/* Create zone list */
	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	dz_if_set_margin(scrolledwindow, 7, 7, 10, 10);
	gtk_container_add(GTK_CONTAINER(frame), scrolledwindow);

	/* Create zone list store and add rows */
	dzd->zlist_store = gtk_list_store_new(DZ_ZONE_LIST_COLUMS,
					      G_TYPE_UINT,
					      G_TYPE_UINT,
					      G_TYPE_UINT,
					      G_TYPE_UINT,
					      G_TYPE_UINT,
					      G_TYPE_UINT64,
					      G_TYPE_UINT64,
					      G_TYPE_UINT64,
					      G_TYPE_INT);
	for (i = 0; i < dzd->max_nr_zones; i++)
		gtk_list_store_append(dzd->zlist_store, &iter);

	dzd->zlist_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(dzd->zlist_store), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(dzd->zlist_model),
					       dz_if_zlist_entry_visible,
					       dzd, NULL);

	/* Initialize the tree view */
	treeview = gtk_tree_view_new_with_model(dzd->zlist_model);
	gtk_widget_show(treeview);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)), GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	dzd->zlist_treeview = treeview;
	dzd->zlist_selection = -1;
	gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
					       dz_if_zlist_select_cb,
					       dzd,
					       NULL);

	g_signal_connect((gpointer) gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(treeview)),
			 "value-changed",
			 G_CALLBACK(dz_if_zlist_scroll_cb),
			 dzd);

	/* Number column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Zone number",
							  renderer, "text",
							  DZ_ZONE_NUM, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_number,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Type column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Type",
							  renderer, "text",
							  DZ_ZONE_TYPE, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_type,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Condition column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Condition",
							  renderer, "text",
							  DZ_ZONE_COND, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_cond,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Need reset column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("RWP Recommended",
							  renderer, "text",
							  DZ_ZONE_RWP_RECOMMENDED, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_rwp_recommended,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Non-seq column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Non Seq",
							  renderer, "text",
							  DZ_ZONE_NONSEQ, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_nonseq,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Start column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Start",
							  renderer, "text",
							  DZ_ZONE_START, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_start,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Length column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Length",
							  renderer, "text",
							  DZ_ZONE_LENGTH, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_length,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Write pointer column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Write Pointer",
							  renderer, "text",
							  DZ_ZONE_WP, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						dz_if_zlist_print_zone_wp,
						dzd, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Fill the model with zone data */
	dz_if_zlist_fill(dzd);

	/* Zone state drawing frame */
	frame = gtk_frame_new("<b>Zone Write State</b>");
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

	g_signal_connect((gpointer) da,
			 "draw",
			 G_CALLBACK(dz_if_zones_draw_legend_cb),
			 dzd);

	da = gtk_drawing_area_new();
	gtk_widget_set_size_request(da, -1, 100);
	gtk_widget_show(da);
	gtk_container_add(GTK_CONTAINER(vbox), da);
	dzd->zones_da = da;

	g_signal_connect((gpointer) da,
			 "draw",
			 G_CALLBACK(dz_if_zones_draw_cb),
			 dzd);

	/* Zone control frame */
	frame = gtk_frame_new("<b>Zone Control</b>");
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, TRUE, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);

	/* Hbox for controls */
	ctrl_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_show(ctrl_hbox);
	dz_if_set_margin(ctrl_hbox, 7, 7, 0, 0);
	gtk_container_add(GTK_CONTAINER(frame), ctrl_hbox);

	/* Zone number selection entry */
	label = gtk_label_new("<b>Zone number</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(ctrl_hbox), label, FALSE, FALSE, 5);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), "-1");
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(ctrl_hbox), entry, FALSE, FALSE, 0);
	dzd->znum_entry = entry;

	g_signal_connect((gpointer)entry,
			 "activate",
			 G_CALLBACK(dz_if_znum_set_cb),
			 dzd);

	/* Block selection entry */
	label = gtk_label_new("<b>Block</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(ctrl_hbox), label, FALSE, FALSE, 5);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), "-1");
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(ctrl_hbox), entry, FALSE, FALSE, 0);
	dzd->zblock_entry = entry;

	g_signal_connect((gpointer)entry,
			 "activate",
			 G_CALLBACK(dz_if_zblock_set_cb),
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

	g_signal_connect((gpointer) button,
			 "clicked",
			 G_CALLBACK(dz_if_zone_open_cb),
			 dzd);

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

	g_signal_connect((gpointer) button,
			 "clicked",
			 G_CALLBACK(dz_if_zone_close_cb),
			 dzd);

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

	g_signal_connect((gpointer) button,
			 "clicked",
			 G_CALLBACK(dz_if_zone_finish_cb),
			 dzd);

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

	g_signal_connect((gpointer) button,
			 "clicked",
			 G_CALLBACK(dz_if_zone_reset_cb),
			 dzd);

	gtk_widget_show_all(dz.window);

	return dzd;
}

void dz_if_dev_close(dz_dev_t *dzd)
{
	/* Close the device */
	dz_close(dzd);
}

void dz_if_dev_update(dz_dev_t *dzd, int do_report_zones)
{
	if (do_report_zones)
		/* Update zones info */
		dz_if_update_zones(dzd);
	else
		/* Redraw viewable zone range */
		dz_if_redraw_zones(dzd);
}

static long long dz_if_sect2block(dz_dev_t *dzd,
				  unsigned long long sector)
{
	return (sector << 9) / dzd->block_size;
}

static void dz_if_zlist_print_zone_number(GtkTreeViewColumn *col,
					  GtkCellRenderer *renderer,
					  GtkTreeModel *model,
					  GtkTreeIter *iter,
					  gpointer user_data)
{
	char str[64];
	int i;

	gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);

	/* Bold black font */
	g_object_set(renderer, "foreground", "Black",
		     "foreground-set", TRUE, NULL);
	g_object_set(renderer, "weight", PANGO_WEIGHT_BOLD,
		     "weight-set", FALSE, NULL);
	snprintf(str, sizeof(str), "%d", i);
	g_object_set(renderer, "text", str, NULL);
}

static void dz_if_zlist_print_zone_type(GtkTreeViewColumn *col,
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
	z = &dzd->zones[i].info;

	/* Normal black font */
	g_object_set(renderer, "foreground", "Black",
		     "foreground-set", TRUE, NULL);
	if (zbc_zone_conventional(z))
		strncpy(str, "Conventional", sizeof(str));
	else if (zbc_zone_sequential_req(z))
		strncpy(str, "Seq write req.", sizeof(str));
	else if (zbc_zone_sequential_pref(z))
		strncpy(str, "Seq write pref.", sizeof(str));
	else
		snprintf(str, sizeof(str), "??? (0x%01x)", zbc_zone_type(z));
	g_object_set(renderer, "text", str, NULL);
}

static void dz_if_zlist_print_zone_cond(GtkTreeViewColumn *col,
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
	z = &dzd->zones[i].info;

	/* Normal black font by default */
	g_object_set(renderer, "foreground", "Black",
		     "foreground-set", TRUE, NULL);
	if (zbc_zone_not_wp(z)) {
		strncpy(str, "Not WP", sizeof(str));
	} else if (zbc_zone_empty(z)) {
		g_object_set(renderer, "foreground", "Green",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Empty", sizeof(str));
	} else if (zbc_zone_full(z)) {
		g_object_set(renderer, "foreground", "Red",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Full", sizeof(str));
	} else if (zbc_zone_imp_open(z)) {
		g_object_set(renderer, "foreground", "Blue",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Implicit Open", sizeof(str));
	} else if (zbc_zone_exp_open(z)) {
		g_object_set(renderer, "foreground", "Blue",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Explicit Open", sizeof(str));
	} else if (zbc_zone_closed(z)) {
		strncpy(str, "Closed", sizeof(str));
	} else if (zbc_zone_rdonly(z)) {
		strncpy(str, "Read-only", sizeof(str));
	} else if (zbc_zone_offline(z)) {
		strncpy(str, "Offline", sizeof(str));
	} else {
		snprintf(str, sizeof(str), "??? (0x%01x)", z->zbz_condition);
	}
	g_object_set(renderer, "text", str, NULL);
}

static void dz_if_zlist_print_zone_rwp_recommended(GtkTreeViewColumn *col,
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
	z = &dzd->zones[i].info;

	/* Zone rwp recommended */
	if (zbc_zone_rwp_recommended(z)) {
		g_object_set(renderer, "foreground", "Red",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Yes", sizeof(str));
	} else {
		g_object_set(renderer, "foreground", "Green",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "No", sizeof(str));
	}
	g_object_set(renderer, "text", str, NULL);
}

static void dz_if_zlist_print_zone_nonseq(GtkTreeViewColumn *col,
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
	z = &dzd->zones[i].info;

	/* Zone non seq */
	if ( zbc_zone_non_seq(z) ) {
		g_object_set(renderer, "foreground", "Red",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Yes", sizeof(str));
	} else {
		g_object_set(renderer, "foreground", "Green",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "No", sizeof(str));
	}
	g_object_set(renderer, "text", str, NULL);
}

static void dz_if_val_str(dz_dev_t *dzd, char *str, long long val)
{
	char *format;

	if (dzd->use_hexa)
		format = "0x%llX";
	else
		format = "%lld";

	sprintf(str, format, dz_if_sect2block(dzd, val));
}

static void dz_if_zlist_print_zone_start(GtkTreeViewColumn *col,
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
	z = &dzd->zones[i].info;

	/* Zone start */
	g_object_set(renderer, "foreground", "Black",
		     "foreground-set", TRUE, NULL);
	dz_if_val_str(dzd, str, zbc_zone_start(z));
	g_object_set(renderer, "text", str, NULL);
}

static void dz_if_zlist_print_zone_length(GtkTreeViewColumn *col,
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
    z = &dzd->zones[i].info;

    /* Zone length */
    g_object_set(renderer, "foreground", "Black", "foreground-set", TRUE, NULL);
    dz_if_val_str(dzd, str, zbc_zone_length(z));
    g_object_set(renderer, "text", str, NULL);
}

static void dz_if_zlist_print_zone_wp(GtkTreeViewColumn *col,
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
	z = &dzd->zones[i].info;

	/* Zone wp LBA */
	if ( zbc_zone_not_wp(z) ) {
		g_object_set(renderer, "foreground", "Grey",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "N/A", sizeof(str));
	} else if ( zbc_zone_empty(z) ) {
		g_object_set(renderer, "foreground", "Green",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Empty", sizeof(str));
	} else if ( zbc_zone_full(z) ) {
		g_object_set(renderer, "foreground", "Red",
			     "foreground-set", TRUE, NULL);
		strncpy(str, "Full", sizeof(str));
	} else {
		g_object_set(renderer, "foreground", "Black",
			     "foreground-set", TRUE, NULL);
		dz_if_val_str(dzd, str, zbc_zone_wp(z));
	}
	g_object_set(renderer, "text", str, NULL);
}

static gboolean dz_if_zlist_entry_visible(GtkTreeModel *model,
					  GtkTreeIter *iter,
					  gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	int i;

	gtk_tree_model_get(model, iter, DZ_ZONE_NUM, &i, -1);

	return dzd->zones[i].visible ? TRUE : FALSE;
}

static void dz_if_zlist_fill(dz_dev_t *dzd)
{
	GtkTreeModel *model = GTK_TREE_MODEL(dzd->zlist_store);
	GtkTreeIter iter;
	struct zbc_zone *z;
	unsigned int i;

	gtk_tree_model_get_iter_first(model, &iter);

	for (i = 0; i < dzd->max_nr_zones; i++) {
		z = &dzd->zones[i].info;
		gtk_list_store_set(dzd->zlist_store, &iter,
				   DZ_ZONE_NUM, dzd->zones[i].no,
				   DZ_ZONE_TYPE, z->zbz_type,
				   DZ_ZONE_COND, z->zbz_condition,
				   DZ_ZONE_RWP_RECOMMENDED, zbc_zone_rwp_recommended(z),
				   DZ_ZONE_NONSEQ, zbc_zone_non_seq(z),
				   DZ_ZONE_START, dz_if_sect2block(dzd, zbc_zone_start(z)),
				   DZ_ZONE_LENGTH, dz_if_sect2block(dzd, zbc_zone_length(z)),
				   DZ_ZONE_WP, dz_if_sect2block(dzd, zbc_zone_wp(z)),
				   DZ_ZONE_VISIBLE, dzd->zones[i].visible,
				   -1);
		gtk_tree_model_iter_next(model, &iter);

	}

	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(dzd->zlist_model));
}

static void dz_if_zlist_update_range(dz_dev_t *dzd)
{
	GtkTreePath *start = NULL, *end = NULL;
	GtkTreeIter iter;

	if (!dzd->nr_zones || !dzd->zones) {
		dzd->zlist_start_no = 0;
		dzd->zlist_end_no = 0;
		return;
	}

	gtk_tree_view_get_visible_range(GTK_TREE_VIEW(dzd->zlist_treeview), &start, &end);
	if (start) {
		if (gtk_tree_model_get_iter(dzd->zlist_model, &iter, start) == TRUE)
			gtk_tree_model_get(dzd->zlist_model, &iter, DZ_ZONE_NUM,
					   &dzd->zlist_start_no, -1);
		else
			dzd->zlist_start_no = 0;
		gtk_tree_path_free(start);
	} else {
		dzd->zlist_start_no = 0;
	}

	if (end) {
		if (gtk_tree_model_get_iter(dzd->zlist_model, &iter, end) == TRUE)
			gtk_tree_model_get(dzd->zlist_model, &iter, DZ_ZONE_NUM,
					   &dzd->zlist_end_no, -1);
		else
			dzd->zlist_end_no = dzd->nr_zones - 1;
		gtk_tree_path_free(end);
	} else {
		dzd->zlist_end_no = dzd->nr_zones - 1;
	}
}

static void dz_if_zlist_set_view_range(dz_dev_t *dzd, int center)
{
	GtkTreePath *path;
	float align;
	int zno = -1;

	if (!dzd->nr_zones)
		return;

	/* Go to zno (center) */
	if (center) {
		zno = dzd->zlist_selection;
		align = 0.5;
	}
	if (zno < 0) {
		zno = dzd->zlist_start_no;
		align = 0.0;
	}

	path = gtk_tree_path_new_from_indices(zno, -1);
	if (path) {
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(dzd->zlist_treeview),
					     path, NULL, TRUE, align, 0.0);
		gtk_tree_path_free(path);
	}
}

static void dz_if_redraw_zones(dz_dev_t *dzd)
{
	/* Re-draw zone state */
	gtk_widget_queue_draw(dzd->zones_da);
}

static void dz_if_zlist_scroll_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;

	/* Update visible zone range in list */
	dz_if_redraw_zones(dzd);
}

static void dz_if_znum_set(dz_dev_t *dzd)
{
	char str[64];

	snprintf(str, sizeof(str) - 1, "%d", dzd->zlist_selection);
	gtk_entry_set_text(GTK_ENTRY(dzd->znum_entry), str);
}

static void dz_if_zblock_set(dz_dev_t *dzd)
{
	long long block;
	char str[64];

	if (dzd->zlist_selection < 0) {
		block = -1;
		strcpy(str, "-1");
	} else {
		block = zbc_zone_start(&dzd->zones[dzd->zlist_selection].info);
		dz_if_val_str(dzd, str, block);
	}
	gtk_entry_set_text(GTK_ENTRY(dzd->zblock_entry), str);
}

static void dz_if_set_selection(dz_dev_t *dzd, int zno)
{
	if (zno == dzd->zlist_selection)
		return;

	dzd->zlist_selection = zno;
	dz_if_znum_set(dzd);
	dz_if_zblock_set(dzd);
}

static gboolean dz_if_zlist_select_cb(GtkTreeSelection *selection,
				      GtkTreeModel *model, GtkTreePath *path,
				      gboolean path_currently_selected,
				      gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	GtkTreeIter iter;
	int zno;

	if (path_currently_selected)
		return TRUE;

	if (!dzd->nr_zones)
		zno = -1;
	else if (gtk_tree_model_get_iter(model, &iter, path))
		gtk_tree_model_get(model, &iter, DZ_ZONE_NUM, &zno, -1);

	dz_if_set_selection(dzd, zno);

	return TRUE;
}

static void dz_if_zlist_clear_selection(dz_dev_t *dzd)
{
	GtkTreeSelection *sel;

	if (dzd->zlist_selection < 0)
		return;

        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dzd->zlist_treeview));
	gtk_tree_selection_unselect_all(sel);
	dz_if_set_selection(dzd, -1);
}

static void dz_if_zlist_set_selection(dz_dev_t *dzd, int zno)
{
	GtkTreeSelection *sel;
	GtkTreePath *path;

	if (zno == dzd->zlist_selection)
		return;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dzd->zlist_treeview));
	path = gtk_tree_path_new_from_indices(zno, -1);
	if (path) {
		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);
	}

	dz_if_set_selection(dzd, zno);
}

static void dz_if_znum_set_cb(GtkEntry *entry, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	int zno;

	zno = atoi(gtk_entry_get_text(entry));
	if (zno < 0 || zno >= (gint)dzd->max_nr_zones) {
		dz_if_zlist_clear_selection(dzd);
		return;
	}

	dz_if_zlist_set_selection(dzd, zno);
	dz_if_zlist_set_view_range(dzd, 1);
}

static void dz_if_zblock_set_cb(GtkEntry *entry, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	long long block = strtoll(gtk_entry_get_text(entry), NULL, 0);
	struct zbc_zone *z;
	unsigned int i;
	int zno = -1;

	if (block >= 0 &&
	    block < dz_if_sect2block(dzd, dzd->info.zbd_lblocks)) {
		/* Search zone */
		for(i = 0; i < dzd->max_nr_zones; i++) {
			z = &dzd->zones[i].info;
			if (block >= dz_if_sect2block(dzd, zbc_zone_start(z)) &&
			    block < dz_if_sect2block(dzd, zbc_zone_start(z) + zbc_zone_length(z))) {
				zno = i;
				break;
			}
		}
	}

	if (zno < 0) {
		dz_if_zlist_clear_selection(dzd);
		return;
	}

	dz_if_zlist_set_selection(dzd, zno);
	dz_if_zlist_set_view_range(dzd, 1);
}

static void dz_if_refresh_zlist(dz_dev_t *dzd)
{
	char str[128];

	/* Update number of zones */
	snprintf(str, sizeof(str) - 1,
		 "<b>%s: %d zones</b>",
		 dzd->path, dzd->nr_zones);
	gtk_label_set_text(GTK_LABEL(dzd->zlist_frame_label), str);
	gtk_label_set_use_markup(GTK_LABEL(dzd->zlist_frame_label), TRUE);

	/* Update list store and refilter the view */
	dz_if_zlist_fill(dzd);

	/* Redraw visible zone range */
	dz_if_redraw_zones(dzd);
}

static void dz_if_update_zones(dz_dev_t *dzd)
{
	GtkWidget *dialog;
	int ret;

	/* Update zone information */
	ret = dz_cmd_exec(dzd, DZ_CMD_REPORT_ZONES,
			  "Getting zone information...");
	if (ret == 0)
		goto out;

	dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					"Get zone information failed\n");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog),
						 "Error %d (%s)",
						 ret, strerror(ret));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

out:
	/* Update list */
	dz_if_refresh_zlist(dzd);
}

static gboolean dz_if_zlist_filter_cb(GtkComboBox *button, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	GtkWidget *combo = dzd->zfilter_combo;
	int zone_ro = ZBC_RO_ALL;
	gchar *text;
	int i = 0;

	text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
	if (!text)
		return FALSE;

	while (zfilter[i].str) {
		if (strcmp(zfilter[i].str, text) == 0) {
			zone_ro = zfilter[i].ro;
			break;
		}
		i++;
	}

	g_free(text);

	if (dzd->zone_ro != zone_ro) {
		dzd->zone_ro = zone_ro;
		dzd->zlist_start_no = 0;
		dzd->zlist_end_no = 0;
		dz_if_update_zones(dzd);
		dz_if_zlist_clear_selection(dzd);
		dz_if_zlist_set_view_range(dzd, 0);
	}

	return FALSE;
}

static void dz_if_zlist_set_block_size_cb(GtkEntry *entry, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	char *val_str = (char *) gtk_entry_get_text(entry);
	int block_size = atoi(val_str);
	char str[32];


	if (!val_str || !dzd)
		return;

	if (block_size > 0)
	    dzd->block_size = block_size;
	else if (dz.block_size)
	    dzd->block_size = dz.block_size;
	else
	    dzd->block_size = dzd->info.zbd_lblock_size;

	snprintf(str, sizeof(str) - 1, "%d", dzd->block_size);
	gtk_entry_set_text(entry, str);

	/* Update list */
	dz_if_zlist_fill(dzd);
	dz_if_zblock_set(dzd);
}

static void dz_if_zlist_set_use_hexa_cb(GtkToggleButton *togglebutton,
					gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	int use_hexa = gtk_toggle_button_get_active(togglebutton);

	if (dzd->use_hexa == use_hexa)
		return;

	/* Update list */
	dzd->use_hexa = use_hexa;
	dz_if_zlist_fill(dzd);
	dz_if_zblock_set(dzd);
}

static gboolean dz_if_zlist_refresh_cb(GtkWidget *widget, gpointer user_data)
{

	dz_if_update_zones((dz_dev_t *) user_data);

	return FALSE;
}

static gboolean dz_if_zones_draw_legend_cb(GtkWidget *widget, cairo_t *cr,
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
	cairo_select_font_face(cr, "Monospace",
			       CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
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
	cairo_move_to(cr,
		      x + 5 - te.x_bearing,
		      h / 2 - te.height / 2 - te.y_bearing);
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
	cairo_move_to(cr,
		      x + 5 - te.x_bearing,
		      h / 2 - te.height / 2 - te.y_bearing);
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
	cairo_move_to(cr,
		      x + 5 - te.x_bearing,
		      h / 2 - te.height / 2 - te.y_bearing);
	cairo_show_text(cr, "Sequential zone written space");

	return FALSE;
}

#define DZ_DRAW_WOFST	5
#define DZ_DRAW_HOFST	20

static gboolean dz_if_zones_draw_cb(GtkWidget *widget, cairo_t *cr,
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
	dz_if_zlist_update_range(dzd);

	if (!dzd->zones || !dzd->nr_zones)
		return FALSE;

	/* Current size */
	gtk_widget_get_allocation(dzd->zones_da, &allocation);
	w = allocation.width - (DZ_DRAW_WOFST * 2);
	h = allocation.height;

	/* Get total viewed capacity */
	if (dzd->zlist_end_no >= dzd->max_nr_zones)
		dzd->zlist_end_no = dzd->max_nr_zones - 1;
	for (i = dzd->zlist_start_no; i <= dzd->zlist_end_no; i++) {
		if (dzd->zones[i].visible)
			cap += zbc_zone_length(&dzd->zones[i].info);
	}

	/* Center overall drawing using x offset */
	zw = 0;
	for (i = dzd->zlist_start_no; i <= dzd->zlist_end_no; i++) {
		if (dzd->zones[i].visible)
			zw += ((unsigned long long)w * zbc_zone_length(&dzd->zones[i].info)) / cap;
	}
	x = DZ_DRAW_WOFST + (w - zw) / 2;

	/* Draw zones */
	for (i = dzd->zlist_start_no; i <= dzd->zlist_end_no; i++) {

		if (!dzd->zones[i].visible)
			continue;

		z = &dzd->zones[i].info;

		/* Draw zone outline */
		zw = (w * zbc_zone_length(z)) / cap;
		gdk_rgba_parse(&color, "Black");
		gdk_cairo_set_source_rgba(cr, &color);
		cairo_set_line_width(cr, 1);
		cairo_rectangle(cr,
				x, DZ_DRAW_HOFST,
				zw, h - (DZ_DRAW_HOFST * 2));
		cairo_stroke_preserve(cr);

		if (zbc_zone_conventional(z))
			gdk_cairo_set_source_rgba(cr, &dz.conv_color);
		else if (zbc_zone_full(z))
			gdk_cairo_set_source_rgba(cr, &dz.seqw_color);
		else
			gdk_cairo_set_source_rgba(cr, &dz.seqnw_color);
		cairo_fill(cr);

		if (!zbc_zone_conventional(z) &&
		    (zbc_zone_imp_open(z) ||
		     zbc_zone_exp_open(z) ||
		     zbc_zone_closed(z))) {
			/* Written space in zone */
			ww = (zw * (zbc_zone_wp(z) - zbc_zone_start(z)))
				/ zbc_zone_length(z);
			if ( ww ) {
				gdk_cairo_set_source_rgba(cr, &dz.seqw_color);
				cairo_rectangle(cr,
						x, DZ_DRAW_HOFST,
						ww, h - DZ_DRAW_HOFST * 2);
				cairo_fill(cr);
			}
		}

		/* Set font */
		gdk_rgba_parse(&color, "Black");
		gdk_cairo_set_source_rgba(cr, &color);
		cairo_select_font_face(cr, "Monospace",
				       CAIRO_FONT_SLANT_NORMAL,
				       CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 10);

		/* Write zone number */
		sprintf(str, "%05d", dzd->zones[i].no);
		cairo_text_extents(cr, str, &te);
		cairo_move_to(cr,
			      x + zw / 2 - te.width / 2 - te.x_bearing,
			      DZ_DRAW_HOFST - te.height / 2);
		cairo_show_text(cr, str);

		/* Write zone size */
		sz = zbc_zone_length(z) * dzd->info.zbd_lblock_size;
		if (sz > 1024 * 1024 *1024)
			sprintf(str, "%llu GiB", sz / (1024 * 1024 *1024));
		else
			sprintf(str, "%llu MiB", sz / (1024 * 1024));
		cairo_set_font_size(cr, 8);
		cairo_text_extents(cr, str, &te);
		cairo_move_to(cr,
			      x + zw / 2 - te.width / 2 - te.x_bearing,
			      h - te.height / 2);
		cairo_show_text(cr, str);

		x += zw;

	}

	return FALSE;
}

static void dz_if_zone_op(dz_dev_t *dzd, enum zbc_zone_op op,
			  char *op_name, char *msg)
{
	GtkWidget *dialog;
	char str[128];
	int ret;

	dzd->zone_no = dzd->zlist_selection;
	dzd->zone_op = op;

	ret = dz_cmd_exec(dzd, DZ_CMD_ZONE_OP, msg);
	if (ret != 0) {

		if (dzd->zone_no == -1)
			sprintf(str, "%s all zones failed\n",
				op_name);
		else
			sprintf(str, "%s zone %d failed\n",
				op_name, dzd->zone_no);

		dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
						GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						str);
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
							 "Error %d (%s)",
							 ret, strerror(ret));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

	}

	/* Update zone list */
	dz_if_refresh_zlist(dzd);
}

static gboolean dz_if_zone_open_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;

	dz_if_zone_op(dzd, ZBC_OP_OPEN_ZONE, "Open", NULL);

	return FALSE;
}

static gboolean dz_if_zone_close_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;

	dz_if_zone_op(dzd, ZBC_OP_CLOSE_ZONE, "Close", NULL);

	return FALSE;
}

static gboolean dz_if_zone_finish_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;

	dz_if_zone_op(dzd, ZBC_OP_FINISH_ZONE, "Finish", NULL);

	return FALSE;
}

static gboolean dz_if_zone_reset_cb(GtkWidget *widget, gpointer user_data)
{
	dz_dev_t *dzd = (dz_dev_t *) user_data;
	char msg[128];

	dz_if_zone_op(dzd, ZBC_OP_RESET_ZONE, "Reset", msg);

	return FALSE;
}

