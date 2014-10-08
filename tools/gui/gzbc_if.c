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
dz_if_expose_cb(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data);

static void
dz_if_delete_cb(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data);

static gboolean
dz_if_exit_cb(GtkWidget *widget,
              GdkEventButton *event,
              gpointer user_data);

static gboolean
dz_if_timer_cb(gpointer user_data);

static gboolean
dz_if_refresh_cb(GtkWidget *widget,
                 GdkEventButton *event,
                 gpointer user_data);

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               GdkEventButton *event,
               gpointer user_data);

static void
dz_if_refresh_zones(void);

static void
dz_if_destroy_zones(void);

static GdkColor dz_colors[] =
    {
        { 0, 65535,     0, 65535 },        /* Magenta -> conv zone */
        { 0,     0, 32768,     0 },        /* Dark green -> free space in seq write zone */
        { 0, 65535,     0,     0 },        /* Red -> used space in seq write zone */
        { 0, 32768,     0, 32768 },        /* Dark violet */
        { 0, 32768,     0, 65535 },        /* Violet      */
        { 0,     0, 32768, 65535 },        /* Light blue  */

        { 0,     0, 65535,     0 },        /* Green       */
        { 0,     0, 32768, 65535 },        /* Light blue  */
        { 0, 65535, 32768,     0 },        /* Orange      */
        { 0, 42496, 38400,     0 },        /* Gold        */
        { 0,     0, 65535, 65535 },        /* Cyan        */
        { 0, 32768, 32768, 32768 },        /* Grey        */
        { 0, 45056,     0,     0 },        /* Dark red    */
        { 0, 65535, 65535, 65535 },        /* White       */
        { 0,     0,     0, 65535 },        /* Blue        */
        { 0, 65535, 32768, 32768 },        /* Pink        */
        { 0, 32768, 49152, 32768 },        /* Bad green   */

    };

/***** Definition of public functions *****/

int
dz_if_create(void)
{
    GtkWidget *win_vbox, *hbox, *ctrl_hbox;
    GtkWidget *notebook;
    GtkWidget *notebook_page;
    GtkWidget *scrolledwindow, *viewport;
    GtkWidget *window;
    GtkWidget *alignment;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *spinbutton;
    GtkWidget *hbuttonbox;
    GtkWidget *da;

    /* Window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ZBC Device Zone State");
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_resize(GTK_WINDOW(window), 1280, 320);
    dz.window = window;

    g_signal_connect((gpointer) window, "delete-event",
                     G_CALLBACK(dz_if_delete_cb),
                     NULL);

    /* Top vbox */
    win_vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(win_vbox);
    gtk_container_add(GTK_CONTAINER(window), win_vbox);

    /* Garbage (for colors) */
    da = gtk_drawing_area_new();
    gtk_widget_show(da);
    gtk_box_pack_start(GTK_BOX(win_vbox), da, FALSE, FALSE, 0);
    dz.zda = da;

    /* Notbook */
    notebook = gtk_notebook_new();
    gtk_widget_show(notebook);
    gtk_box_pack_start(GTK_BOX(win_vbox), notebook, TRUE, TRUE, 0);
    dz.notebook = notebook;

    /* Notbook zone state page */
    notebook_page = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(notebook_page);
    gtk_container_add(GTK_CONTAINER(notebook), notebook_page);
    gtk_container_set_border_width(GTK_CONTAINER(notebook_page), 10);

    label = gtk_label_new ("<b>Zone State</b>");
    gtk_widget_show(label);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook), gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0), label);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow);
    gtk_container_add(GTK_CONTAINER(notebook_page), scrolledwindow);

    viewport = gtk_viewport_new(NULL, NULL);
    gtk_widget_show(viewport);
    gtk_container_set_border_width(GTK_CONTAINER(viewport), 10);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);

    dz.zstate_container = viewport;

    /* Notbook zone info page */
    notebook_page = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(notebook_page);
    gtk_container_add(GTK_CONTAINER(notebook), notebook_page);
    gtk_container_set_border_width(GTK_CONTAINER(notebook_page), 10);

    label = gtk_label_new ("<b>Zone Information</b>");
    gtk_widget_show(label);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook), gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1), label);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow);
    gtk_container_add(GTK_CONTAINER(notebook_page), scrolledwindow);

    viewport = gtk_viewport_new(NULL, NULL);
    gtk_widget_show(viewport);
    gtk_container_set_border_width(GTK_CONTAINER(viewport), 10);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);
    dz.zinfo_container = viewport;

    /* Hbox for controls */
    ctrl_hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(ctrl_hbox);
    gtk_box_pack_start(GTK_BOX(win_vbox), ctrl_hbox, FALSE, FALSE, 0);

    /* Reset zone controls: label, spinbutton and button */
    label = gtk_label_new("<b>Zone index</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), label, FALSE, FALSE, 0);

    dz.zadj = (GtkAdjustment *) gtk_adjustment_new(0, 0, 0, 1, 10, 0);
    spinbutton = gtk_spin_button_new(GTK_ADJUSTMENT(dz.zadj), 1, 0);
    gtk_widget_show(spinbutton);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(ctrl_hbox), spinbutton, FALSE, FALSE, 0);

    /* Zone control button Box */
    hbuttonbox = gtk_hbutton_box_new();
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

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);

    image = gtk_image_new_from_stock("gtk-clear", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Reset Write Pointer");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_reset_cb),
                      spinbutton);

    /* Exit, refresh, ... button Box */
    hbuttonbox = gtk_hbutton_box_new();
    gtk_widget_show(hbuttonbox);
    gtk_box_pack_end(GTK_BOX(ctrl_hbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), 10);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbuttonbox), 10);

    if ( ! dz.interval ) {

        /* Refresh button */
        button = gtk_button_new();
        gtk_widget_show(button);
        gtk_container_add(GTK_CONTAINER(hbuttonbox), button);
        GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

        alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
        gtk_widget_show(alignment);
        gtk_container_add(GTK_CONTAINER(button), alignment);

        hbox = gtk_hbox_new(FALSE, 2);
        gtk_widget_show(hbox);
        gtk_container_add(GTK_CONTAINER(alignment), hbox);

        image = gtk_image_new_from_stock("gtk-refresh", GTK_ICON_SIZE_BUTTON);
        gtk_widget_show(image);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

        label = gtk_label_new_with_mnemonic("Refresh");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        g_signal_connect((gpointer) button, "button_press_event",
                         G_CALLBACK(dz_if_refresh_cb),
                         &dz);

    }

    /* Exit button */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox), button);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

    alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_widget_show(alignment);
    gtk_container_add(GTK_CONTAINER(button), alignment);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);

    image = gtk_image_new_from_stock("gtk-quit", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic("Exit");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer) button, "button_press_event",
                      G_CALLBACK(dz_if_exit_cb),
                      &dz);

    if ( dz.interval ) {
        /* Add timer */
        dz.timer_id = g_timeout_add(dz.interval, dz_if_timer_cb, NULL);
    }

    /* Done */
    gtk_widget_show(dz.window);

    /* Update drawing area */
    dz_if_refresh_zones();

    return( 0 );

}

void
dz_if_destroy(void)
{

    dz_if_destroy_zones();

    if ( dz.window ) {
        gtk_widget_destroy(dz.window);
        dz.window = NULL;
    }

    if ( dz.conv_gc ) {
        g_object_unref(dz.conv_gc);
        dz.conv_gc = NULL;
    }

    if ( dz.free_gc ) {
        g_object_unref(dz.free_gc);
        dz.free_gc = NULL;
    }

    if ( dz.used_gc ) {
        g_object_unref(dz.used_gc);
        dz.used_gc = NULL;
    }

    if ( dz.timer_id ) {
        g_source_remove(dz.timer_id);
        dz.timer_id = 0;
    }

    return;

}

/***** Definition of private functions *****/

static char *dz_info_label[DZ_ZONE_INFO_FIELD_NUM] =
    {
        "Index",
        "Type",
        "Condition",
        "Flags",
        "Start LBA",
        "Length",
        "Write pointer LBA"
    };

static void
dz_if_refresh_zones_info(void)
{
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *entry;
    char str[64];
    int i, z;

    if ( ! dz.zinfo_table ) {

        /* Table container */
        table = gtk_table_new(dz.nr_zones + 1, 7, FALSE);
        gtk_widget_show(table);
        gtk_container_set_border_width(GTK_CONTAINER(table), 10);
        gtk_table_set_col_spacings(GTK_TABLE(table), 10);
        gtk_container_add(GTK_CONTAINER(dz.zinfo_container), table);
        dz.zinfo_table = table;

        /* Info labels */
        for(i = 0; i < DZ_ZONE_INFO_FIELD_NUM; i++) {
            sprintf(str, "<b>%s</b>", dz_info_label[i]);
            label = gtk_label_new(str);
            gtk_label_set_text(GTK_LABEL(label), str);
            gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
            gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
            gtk_widget_show(label);
            gtk_table_attach(GTK_TABLE(table), label, i, i + 1, 0, 1,
                             (GtkAttachOptions) (GTK_FILL),
                             (GtkAttachOptions) 0,
                             0, 0);
        }

        /* Zone list */
        for(z = 0; z < dz.nr_zones; z++) {

            /* Line header */
            sprintf(str, "<b>%03d</b>", z);
            label = gtk_label_new(str);
            gtk_label_set_text(GTK_LABEL(label), str);
            gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
            gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
            gtk_widget_show(label);
            gtk_table_attach(GTK_TABLE(table), label, 0, 1, z + 1, z + 2,
                             (GtkAttachOptions) (GTK_FILL),
                             (GtkAttachOptions) 0,
                             0, 0);

            /* Line fields */
            for(i = 0; i < DZ_ZONE_INFO_FIELD_NUM - 1; i++) {
                entry = gtk_entry_new();
                gtk_widget_show(entry);
                gtk_table_attach(GTK_TABLE(table), entry, i + 1, i + 2, z + 1, z + 2,
                                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                                 (GtkAttachOptions) 0,
                                 0, 0);
                dz.z[z].entry[i] = entry;
            }

        }

    }

    /* Update values */
    for(z = 0; z < dz.nr_zones; z++) {

        sprintf(str, "0x%01x", dz.zones[z].zbz_type);
        gtk_entry_set_text(GTK_ENTRY(dz.z[z].entry[0]), str);

        sprintf(str, "0x%01x", dz.zones[z].zbz_condition);
        gtk_entry_set_text(GTK_ENTRY(dz.z[z].entry[1]), str);

        sprintf(str, "0x%02x", dz.zones[z].zbz_flags);
        gtk_entry_set_text(GTK_ENTRY(dz.z[z].entry[2]), str);

        sprintf(str, "%llu", zbc_zone_start_lba(&dz.zones[z]));
        gtk_entry_set_text(GTK_ENTRY(dz.z[z].entry[4]), str);

        sprintf(str, "%llu", zbc_zone_length(&dz.zones[z]));
        gtk_entry_set_text(GTK_ENTRY(dz.z[z].entry[5]), str);

        sprintf(str, "%llu", zbc_zone_wp_lba(&dz.zones[z]));
        gtk_entry_set_text(GTK_ENTRY(dz.z[z].entry[6]), str);

    }

    return;

}

static void
dz_if_init_colors(void)
{

    /* Colors */
    if ( ! dz.conv_gc ) {
        dz.conv_gc = gdk_gc_new(dz.zda->window);
        gdk_gc_copy(dz.conv_gc, dz.zda->style->white_gc);
        gdk_gc_set_line_attributes(dz.conv_gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
        gdk_gc_set_rgb_fg_color(dz.conv_gc, &dz_colors[DZ_COLOR_CONV]);
    }

    if ( ! dz.free_gc ) {
        dz.free_gc = gdk_gc_new(dz.zda->window);
        gdk_gc_copy(dz.free_gc, dz.zda->style->white_gc);
        gdk_gc_set_line_attributes(dz.free_gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
        gdk_gc_set_rgb_fg_color(dz.free_gc, &dz_colors[DZ_COLOR_FREE]);
    }

    if ( ! dz.used_gc ) {
        dz.used_gc = gdk_gc_new(dz.zda->window);
        gdk_gc_copy(dz.used_gc, dz.zda->style->white_gc);
        gdk_gc_set_line_attributes(dz.used_gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
        gdk_gc_set_rgb_fg_color(dz.used_gc, &dz_colors[DZ_COLOR_USED]);
    }

    return;

}

static void
dz_if_refresh_zone_state(dz_zone_t *z)
{
    struct zbc_zone *zone = &dz.zones[z->idx];
    gint w, uw, fw, h;
    double used;

    /* Current size */
    w = gdk_window_get_width(z->da->window);
    h = gdk_window_get_height(z->da->window);

    if ( zbc_zone_wp_lba(zone) > zbc_zone_start_lba(zone) ) {
        used = (double)(zbc_zone_wp_lba(zone) - zbc_zone_start_lba(zone)) / (double)zbc_zone_length(zone);
    } else {
        used = 0.0;
    }

    uw = w * used;
    fw = w - uw;
    if ( fw < 0 ) {
        fw = 0;
        uw = w;
    }

    if ( zbc_zone_conventional(zone) ) {

         gdk_draw_rectangle(z->da->window,
                           dz.conv_gc,
                           TRUE,
                           0, 0,
                           w, h);

    } else {

        /* Draw used (red) */
        if ( uw ) {
            gdk_draw_rectangle(z->da->window,
                               dz.used_gc,
                               TRUE,
                               0, 0,
                               uw, h);
        }

        /* Draw free (green) */
        if ( fw ) {
            gdk_draw_rectangle(z->da->window,
                               dz.free_gc,
                               TRUE,
                               uw, 0,
                               w, h);
        }

    }

    /* Draw black rectangle around the zone */
    gdk_draw_rectangle(z->da->window,
                       z->da->style->black_gc,
                       FALSE,
                       0, 0,
                       w, h);

    return;

}

static void
dz_if_refresh_zones_state(void)
{
    int i;

    /* Redraw zones */
    for(i = 0; i < dz.nr_zones; i++) {
        dz_if_refresh_zone_state(&dz.z[i]);
    }

    return;

}

static void
dz_if_create_zone(int idx)
{
    unsigned long long cap;
    struct zbc_zone *zone = &dz.zones[idx];
    dz_zone_t *z = &dz.z[idx];
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *da;
    char str[16];
    int w, h;

    /* Set zone */
    z->idx = idx;
    z->length = zone->zbz_length;

    /* Container */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(dz.zstate_hbox), vbox, TRUE, TRUE, 0);

    /* Zone number label */
    sprintf(str, "<b>%d</b>", idx);
    label = gtk_label_new(str);
    gtk_label_set_text(GTK_LABEL(label), str);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    /* Drawing area */
    da = gtk_drawing_area_new();
    gtk_widget_show(da);
    gtk_box_pack_start(GTK_BOX(vbox), da, TRUE, TRUE, 0);
    z->da = da;

    g_signal_connect((gpointer) da, "expose_event",
                     G_CALLBACK(dz_if_expose_cb),
                     z);

    /* Zone size label */
    cap = (zbc_zone_length(zone) * dz.info.zbd_physical_block_size) / 1000000;
    sprintf(str, "<b>%llu.%01llu</b>",
            cap / 1000,
            (cap % 1000) / 100);
    label = gtk_label_new(str);
    gtk_label_set_text(GTK_LABEL(label), str);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    //w = gdk_window_get_width(z->da->window);
    w = dz.zstate_base_width * ((double)zbc_zone_length(zone) / (double)dz.info.zbd_physical_blocks);
    h = gdk_window_get_height(z->da->window);
    gtk_widget_set_size_request(z->da, w, h);

    return;

}

static void
dz_if_create_zones(void)
{
    GtkAllocation *alloc = g_new(GtkAllocation, 1);
    GtkWidget *vbox, *label;
    uint64_t min_cap;
    int i, w;

    /* Zone list hbox */
    dz.zstate_hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(dz.zstate_hbox);
    gtk_box_set_spacing(GTK_BOX(dz.zstate_hbox), 0);
    gtk_container_set_border_width(GTK_CONTAINER(dz.zstate_hbox), 10);
    gtk_container_add(GTK_CONTAINER(dz.zstate_container), dz.zstate_hbox);

    /* Header container */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(dz.zstate_hbox), vbox, FALSE, TRUE, 0);

    /* Zone number label */
    label = gtk_label_new("<b>Zone index</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    /* Zone state label */
    label = gtk_label_new( "<b>Zone state</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    /* Zone size label */
    label = gtk_label_new("<b>Zone size (GB) </b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    /* Calculate minimum da base size based on zone capacity ratio */
    min_cap = dz.info.zbd_physical_blocks;
    for(i = 0; i < dz.nr_zones; i++) {
        if ( dz.zones[i].zbz_length < min_cap ) {
            min_cap = dz.zones[i].zbz_length;
        }
    }

    gtk_widget_get_allocation(dz.zstate_container, alloc);
    dz.zstate_base_width = alloc->width;
    gtk_widget_get_allocation(label, alloc);
    dz.zstate_base_width -= alloc->width;

    w = dz.zstate_base_width * ((double)min_cap / (double)dz.info.zbd_physical_blocks);
    if ( w < 120 ) {
        dz.zstate_base_width = dz.nr_zones * 120;
    }

    g_free(alloc);

    /* Create zones */
    for(i = 0; i < dz.nr_zones; i++) {
        dz_if_create_zone(i);
    }

    return;

}

static void
dz_if_destroy_zones(void)
{

    if ( dz.window ) {

        if ( dz.zstate_hbox ) {
            gtk_widget_destroy(dz.zstate_hbox);
            dz.zstate_hbox = NULL;
        }

        if ( dz.zinfo_table ) {
            gtk_widget_destroy(dz.zinfo_table);
            dz.zinfo_table = NULL;
        }

    }

    if ( dz.z ) {
        free(dz.z);
        dz.z = NULL;
        dz.nz = 0;
    }

    return;

}

static int
dz_if_zone_changed(void)
{
    int i;

    if ( dz.nr_zones
         && (dz.nr_zones != dz.nz) ) {
        return( 1 );
    }

    for(i = 0; i < dz.nr_zones; i++) {
        if ( dz.z[i].length != dz.zones[i].zbz_length ) {
            return( 1 );
        }
    }

    return( 0 );

}

static void
dz_if_refresh_zones(void)
{
    GtkWidget *dialog;
    int ret;

    /* Colors */
    dz_if_init_colors();

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
    }

    if ( dz_if_zone_changed() ) {

        /* Clear old GUI zone list */
        dz_if_destroy_zones();

        /* Alloc new GUI zone list */
        dz.nz = dz.nr_zones;
        dz.z = (dz_zone_t *) malloc(sizeof(dz_zone_t) * dz.nz);
        if ( ! dz.z ) {
            fprintf(stderr, "No memory\n");
            exit(1);
        }
        memset(dz.z, 0, sizeof(dz_zone_t) * dz.nz);

        /* Set zone selector */
        gtk_adjustment_configure(dz.zadj, 0, 0, dz.nz - 1, 1, 10, 0);

    }

    if ( ! dz.zstate_hbox ) {
        /* Create zones state */
        dz_if_create_zones();
    }

    if ( gtk_notebook_get_current_page(GTK_NOTEBOOK(dz.notebook)) == 0 ) {
        /* Redraw zone state */
        dz_if_refresh_zones_state();
    }

    if ( (gtk_notebook_get_current_page(GTK_NOTEBOOK(dz.notebook)) == 1)
         || (! dz.zinfo_table) ) {
        /* Update zone info */
        dz_if_refresh_zones_info();
    }

    return;

}

static gboolean
dz_if_expose_cb(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data)
{

    dz_if_refresh_zone_state((dz_zone_t *) user_data);

    return( FALSE );

}

static void
dz_if_delete_cb(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data)
{

    dz.window = NULL;

    gtk_main_quit();

    return;

}

static gboolean
dz_if_exit_cb(GtkWidget *widget,
              GdkEventButton *event,
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
dz_if_refresh_cb(GtkWidget *widget,
                 GdkEventButton *event,
                 gpointer user_data)
{

    dz_if_refresh_zones();

    return( FALSE );

}

static gboolean
dz_if_reset_cb(GtkWidget *widget,
               GdkEventButton *event,
               gpointer user_data)
{
    GtkWidget *spinbutton = (GtkWidget *) user_data;
    int idx = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
    GtkWidget *dialog;
    int ret;

    ret = dz_reset_zone(idx);
    if ( ret != 0 ) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Reset zone write pointer failed\n");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog), "Reset zone %d failed %d (%s)",
                                                 idx,
                                                 errno,
                                                 strerror(errno));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    dz_if_refresh_zones();

    return( FALSE );

}
