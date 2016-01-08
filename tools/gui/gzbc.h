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
#include <pthread.h>

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

/**
 * Device command IDs.
 */
enum {
    DZ_CMD_REPORT_ZONES,
    DZ_CMD_RESET_ZONE,
    DZ_CMD_OPEN_ZONE,
    DZ_CMD_CLOSE_ZONE,
    DZ_CMD_FINISH_ZONE,
};

/**
 * Maximum number of devices that can be open.
 */
#define DZ_MAX_DEV	32

/***** Type definitions *****/

/**
 * GUI Tab data.
 */
typedef struct dz_dev {

    char                        path[128];
    int				opening;

    struct zbc_device           *dev;
    struct zbc_device_info      info;
    int                         block_size;

    int                         zone_ro;
    int                         zone_no;
    unsigned int                max_nr_zones;
    unsigned int                nr_zones;
    struct zbc_zone             *zones;

    /**
     * Command execution.
     */
    int				cmd_id;
    int				cmd_do_report_zones;
    pthread_t			cmd_thread;
    GtkWidget                   *cmd_dialog;

    /**
     * Interface stuff.
     */
    GtkWidget                   *page;
    GtkWidget                   *page_frame;

    GtkWidget                   *zfilter_combo;
    GtkWidget                   *zinfo_spinbutton;
    GtkWidget                   *zinfo_frame_label;
    GtkWidget                   *zinfo_treeview;
    GtkTreeModel                *zinfo_model;
    GtkListStore                *zinfo_store;
    unsigned int		zinfo_start_no;
    unsigned int		zinfo_end_no;
    int		                zinfo_selection;

    GtkWidget                   *zstate_da;

} dz_dev_t;

/**
 * GUI data.
 */
typedef struct dz {

    dz_dev_t			dev[DZ_MAX_DEV];
    int				nr_devs;

    int                         interval;
    int                         block_size;
    int                         abort;

    /**
     * Interface stuff.
     */
    GtkWidget                   *window;
    GtkWidget                   *vbox;
    GtkWidget                   *notebook;
    GtkWidget                   *no_dev_frame;

    GdkRGBA			conv_color;
    GdkRGBA			seqnw_color;
    GdkRGBA			seqw_color;

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

extern dz_dev_t *
dz_open(char *path);

extern void
dz_close(dz_dev_t *dzd);

extern int
dz_cmd_exec(dz_dev_t *dzd,
	    int cmd_id,
	    int do_report_zones,
	    char *msg);

extern int
dz_if_create(void);

extern void
dz_if_destroy(void);

extern void
dz_if_add_device(char *dev_path);

extern dz_dev_t *
dz_if_dev_open(char *path);

extern void
dz_if_dev_close(dz_dev_t *dzd);

extern void
dz_if_dev_update(dz_dev_t *dzd,
		 int do_report_zones);

#endif /* __GZBC_H__ */
