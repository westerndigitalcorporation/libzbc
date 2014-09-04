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
 * Colors.
 */
enum {
    DZ_COLOR_CONV = 0,
    DZ_COLOR_FREE,
    DZ_COLOR_USED,
};

/**
 * Number of fields in zone info list.
 */
#define DZ_ZONE_INFO_FIELD_NUM  7

/***** Type definitions *****/

/**
 * Demo GUI control.
 */
typedef struct dz_zone {

    /**
     * Zone index and length.
     */
    int                 idx;
    uint64_t            length;

    /**
     * Interface stuff.
     */
    GtkWidget           *da;
    GtkWidget           *entry[DZ_ZONE_INFO_FIELD_NUM - 1];

} dz_zone_t;

/**
 * Demo control.
 */
typedef struct dz {

    char                        *filename;

    struct zbc_device           *dev;
    struct zbc_device_info      info;

    unsigned int                nr_zones;
    struct zbc_zone             *zones;

    int                         interval;
    int                         abort;

    /**
     * Interface stuff.
     */
    GtkWidget                   *window;
    GtkWidget                   *zda;
    GtkAdjustment               *zadj;
    GtkWidget                   *notebook;

    GdkGC                       *conv_gc;
    GdkGC                       *free_gc;
    GdkGC                       *used_gc;

    GtkWidget                   *zstate_container;
    GtkWidget                   *zstate_hbox;
    int                         zstate_base_width;

    GtkWidget                   *zinfo_container;
    GtkWidget                   *zinfo_table;

    dz_zone_t                   *z;
    int                         nz;

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
dz_reset_zone(int idx);

extern int
dz_if_create(void);

extern void
dz_if_destroy(void);

#endif /* __GZBC_H__ */
