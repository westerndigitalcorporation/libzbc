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
 * Number of fields in the zone info list.
 */
#define DZ_ZONE_INFO_FIELD_NUM  7

/**
 * Initial number of visible lines in the zone info list.
 */
#define DZ_ZONE_INFO_LINE_NUM  	10

/***** Type definitions *****/

/**
 * Zone info line.
 */
typedef struct dz_zinfo_line {

    GtkWidget                   *label;
    GtkWidget                   *entry[DZ_ZONE_INFO_FIELD_NUM];

} dz_zinfo_line_t;

/**
 * GUI data.
 */
typedef struct dz {

    char                        *path;

    struct zbc_device           *dev;
    struct zbc_device_info      info;

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

    GtkWidget                   *zinfo_frame_label;
    GtkWidget                   *zinfo_viewport;
    GtkWidget                   *zinfo_grid;
    int				zinfo_height;
    int				zinfo_line_height;
    int				zinfo_nr_lines;
    dz_zinfo_line_t		*zinfo_lines;
    int				zinfo_zno;
    GtkAdjustment               *zinfo_vadj;

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
dz_reset_zone(int zno);

extern int
dz_if_create(void);

extern void
dz_if_destroy(void);

#endif /* __GZBC_H__ */
