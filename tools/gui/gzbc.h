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

#ifndef __GZBC_H__
#define __GZBC_H__

/***** Including files *****/

#include <sys/time.h>

#include <libzbc/zbc.h>

#include <gtk/gtk.h>

/***** Macro definitions *****/

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
    DZ_ZONE_NEED_RESET,
    DZ_ZONE_NONSEQ,
    DZ_ZONE_START,
    DZ_ZONE_LENGTH,
    DZ_ZONE_WP,
    DZ_ZONE_LIST_COLUMS
};


/***** Type definitions *****/

/**
 * GUI data.
 */
typedef struct dz {

    char                        *path;

    struct zbc_device           *dev;
    struct zbc_device_info      info;

    int                         zone_ro;
    unsigned int                max_nr_zones;
    unsigned int                nr_zones;
    struct zbc_zone             *zones;

    int                         interval;
    int                         block_size;
    int                         abort;

    /**
     * Interface stuff.
     */
    GtkWidget                   *window;
    GtkWidget                   *notebook;

    GdkRGBA			conv_color;
    GdkRGBA			seqnw_color;
    GdkRGBA			seqw_color;

    GtkWidget                   *zinfo_spinbutton;
    GtkWidget                   *zinfo_frame_label;
    GtkWidget                   *zinfo_treeview;
    GtkTreeModel                *zinfo_model;
    GtkListStore                *zinfo_store;
    unsigned int		zinfo_start_no;
    unsigned int		zinfo_end_no;
    unsigned int		zinfo_lines;

    GtkWidget                   *zstate_da;

    /**
     * For handling timer and signals.
     */
    guint                       timer_id;
    int                         sig_pipe[2];

} dz_t;

/***** Declaration of public functions *****/

/**
 * System time in usecs.
 */
static __inline__ unsigned long long
dz_usec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return( (unsigned long long) tv.tv_sec * 1000000LL + (unsigned long long) tv.tv_usec );

}

/***** Declaration of public data *****/

extern dz_t dz;

/***** Declaration of public functions *****/

extern int
dz_get_zones(void);

extern int
dz_open_zone(int zno);

extern int
dz_close_zone(int zno);

extern int
dz_finish_zone(int zno);

extern int
dz_reset_zone(int zno);

extern int
dz_if_create(void);

extern void
dz_if_destroy(void);

#endif /* __GZBC_H__ */
