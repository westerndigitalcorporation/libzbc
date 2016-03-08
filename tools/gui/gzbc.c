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
 */

/***** Including files *****/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>

#include "gzbc.h"

/***** Local data and functions *****/

/**
 * Demo control.
 */
dz_t dz;

/**
 * Signal handling.
 */

static gboolean
dz_process_signal(GIOChannel *source,
                    GIOCondition condition,
                    gpointer user_data)
{
    char buf[32];
    ssize_t size;
    gboolean ret = FALSE;

    if ( condition & G_IO_IN ) {
        size = read(g_io_channel_unix_get_fd(source), buf, sizeof(buf));
        if ( size > 0 ) {
            /* Got signal */
            gtk_main_quit();
            ret = TRUE;
        }
    }

    return( ret );

}

static void
dz_sig_handler(int sig)
{

    /* Propagate signal through the pipe */
    if ( write(dz.sig_pipe[1], &sig, sizeof(int)) < 0 ) {
        printf("Signal %d processing failed\n", sig);
    }

    return;

}

static void
dz_set_signal_handlers(void)
{
    GIOChannel *sig_channel;
    long fd_flags;
    int ret;

    ret = pipe(dz.sig_pipe);
    if ( ret < 0 ) {
        perror("pipe");
        exit(1);
    }

    fd_flags = fcntl(dz.sig_pipe[1], F_GETFL);
    if ( fd_flags < 0 ) {
        perror("Read descriptor flags");
        exit(1);
    }
    ret = fcntl(dz.sig_pipe[1], F_SETFL, fd_flags | O_NONBLOCK);
    if ( ret < 0 ) {
        perror("Write descriptor flags");
        exit(1);
    }

    /* Install the unix signal handler */
    signal(SIGINT, dz_sig_handler);
    signal(SIGQUIT, dz_sig_handler);
    signal(SIGTERM, dz_sig_handler);

    /* Convert the reading end of the pipe into a GIOChannel */
    sig_channel = g_io_channel_unix_new(dz.sig_pipe[0]);
    g_io_channel_set_encoding(sig_channel, NULL, NULL);
    g_io_channel_set_flags(sig_channel, g_io_channel_get_flags(sig_channel) | G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(sig_channel, G_IO_IN | G_IO_PRI, dz_process_signal, NULL);

    return;

}

/***** Main *****/

int
main(int argc,
     char **argv)
{
    gboolean init_ret;
    gboolean verbose = FALSE;
    GError *error = NULL;
    int i;
    GOptionEntry options[] = {
        {
            "interval", 'i', 0,
            G_OPTION_ARG_INT, &dz.interval,
            "Refresh interval (milliseconds)", NULL
        },
	{
            "block", 'b', 0,
            G_OPTION_ARG_INT, &dz.block_size,
            "Use block bytes as the unit for displaying zone LBA, length and write pointer position", NULL
        },
        {
            "verbose", 'v', 0,
            G_OPTION_ARG_NONE, &verbose,
            "Set libzbc verbose mode", NULL
        },
        { NULL }
    };

    /* Init */
    memset(&dz, 0, sizeof(dz));
    init_ret = gtk_init_with_args(&argc,
                                  &argv,
                                  "ZBC device zone state GUI",
                                  options,
                                  NULL,
                                  &error);
    if ( (init_ret == FALSE)
         || (error != NULL) ) {
        printf("Failed to parse command line arguments: %s\n",
               error->message);
        g_error_free(error);
        return( 1 );
    }

    if ( dz.interval < 0 ) {
        fprintf(stderr, "Invalid update interval\n");
        return( 1 );
    }

    if ( dz.block_size < 0 ) {
	fprintf(stderr, "Invalid block size\n");
	return( 1 );
    }

    if ( verbose ) {
        zbc_set_log_level("debug");
    }

    dz_set_signal_handlers();

    /* Create GUI */
    dz_if_create();

    /* Add devices listed on command line */
    for(i = 1; i < argc; i++ ) {
	dz_if_add_device(argv[i]);
    }

    /* Main event loop */
    gtk_main();

    /* Cleanup GUI */
    dz_if_destroy();

    return( 0 );

}

/**
 * Report zones.
 */
static int
dz_report_zones(dz_dev_t *dzd)
{
    unsigned int i, j = 0;
    int ret;

    if ( (! dzd->zones)
	 || (! dzd->max_nr_zones) ) {

	/* Get zone list */
        dzd->zone_ro = ZBC_RO_ALL;
	ret = zbc_list_zones(dzd->dev, 0, dzd->zone_ro, &dzd->zbc_zones, &dzd->nr_zones);
	if ( ret != 0 ) {
	    return( ret );
	}

	/* Allocate zone array */
	dzd->max_nr_zones = dzd->nr_zones;
	dzd->zones = (dz_dev_zone_t *) malloc(sizeof(dz_dev_zone_t) * dzd->max_nr_zones);
	if ( ! dzd->zones ) {
	    return( -ENOMEM );
	}

	for(i = 0; i < dzd->max_nr_zones; i++) {
	    dzd->zones[i].no = i;
	    dzd->zones[i].show = 1;
	    memcpy(&dzd->zones[i].info, &dzd->zbc_zones[i], sizeof(struct zbc_zone));
	}

	return( 0 );

    }

    /* Refresh zone list */
    dzd->nr_zones = dzd->max_nr_zones;
    ret = zbc_report_zones(dzd->dev, 0, dzd->zone_ro, dzd->zbc_zones, &dzd->nr_zones);
    if ( ret != 0 ) {
	fprintf(stderr, "Get zone information failed %d (%s)\n",
		errno,
		strerror(errno));
	dzd->nr_zones = 0;
    }

    /* Apply filter */
    for(i = 0; i < dzd->max_nr_zones; i++) {
	if ( (j < dzd->nr_zones)
	     && (zbc_zone_start_lba(&dzd->zones[i].info) == zbc_zone_start_lba(&dzd->zbc_zones[j])) ) {
	    memcpy(&dzd->zones[i].info, &dzd->zbc_zones[j], sizeof(struct zbc_zone));
	    dzd->zones[i].show = 1;
	    j++;
	} else {
	    dzd->zones[i].show = 0;
	}
    }

    return( ret );

}

/**
 * Reset zone.
 */
static int
dz_reset_zone(dz_dev_t *dzd)
{
    int zno = dzd->zone_no;
    int ret;

    if ( zno == -1 ) {
        ret = zbc_reset_write_pointer(dzd->dev, -1);
    } else if ( (zno >= 0) && (zno < (int)dzd->nr_zones) ) {
        ret = zbc_reset_write_pointer(dzd->dev, zbc_zone_start_lba(&dzd->zones[zno].info));
    } else {
	fprintf(stderr, "Invalid zone number %d for reset\n",
		zno);
	ret = -1;
	errno = EINVAL;
    }

    if ( ret != 0 ) {
        ret = errno;
        fprintf(stderr, "zbc_reset_write_pointer failed %d (%s)\n",
                errno,
                strerror(errno));
    }

    return( ret );

}

/**
 * Open zone.
 */
static int
dz_open_zone(dz_dev_t *dzd)
{
    int zno = dzd->zone_no;
    int ret = 0;

    if ( zno == -1 ) {
        ret = zbc_open_zone(dzd->dev, -1);
    } else if ( (zno >= 0) && (zno < (int)dzd->nr_zones) ) {
        ret = zbc_open_zone(dzd->dev, zbc_zone_start_lba(&dzd->zones[zno].info));
    } else {
	fprintf(stderr, "Invalid zone number %d for open\n",
		zno);
	ret = -1;
	errno = EINVAL;
    }

    if ( ret != 0 ) {
        ret = errno;
        fprintf(stderr, "zbc_open_zone failed %d (%s)\n",
                errno,
                strerror(errno));
    }

    return( ret );

}

/**
 * Close zone.
 */
static int
dz_close_zone(dz_dev_t *dzd)
{
    int zno = dzd->zone_no;
    int ret = 0;

    if ( zno == -1 ) {
        ret = zbc_close_zone(dzd->dev, -1);
    } else if ( (zno >= 0) && (zno < (int)dzd->nr_zones) ) {
        ret = zbc_close_zone(dzd->dev, zbc_zone_start_lba(&dzd->zones[zno].info));
    } else {
	fprintf(stderr, "Invalid zone number %d for close\n",
		zno);
	ret = -1;
	errno = EINVAL;
    }

    if ( ret != 0 ) {
        ret = errno;
        fprintf(stderr, "zbc_close_zone failed %d (%s)\n",
                errno,
                strerror(errno));
    }

    return( ret );

}

/**
 * Finish zone.
 */
static int
dz_finish_zone(dz_dev_t *dzd)
{
    int zno = dzd->zone_no;
    int ret = 0;

    if ( zno == -1 ) {
        ret = zbc_finish_zone(dzd->dev, -1);
    } else if ( (zno >= 0) && (zno < (int)dzd->nr_zones) ) {
        ret = zbc_finish_zone(dzd->dev, zbc_zone_start_lba(&dzd->zones[zno].info));
    } else {
	fprintf(stderr, "Invalid zone number %d for close\n",
		zno);
	ret = -1;
	errno = EINVAL;
    }

    if ( ret != 0 ) {
        ret = errno;
        fprintf(stderr, "zbc_finish_zone failed %d (%s)\n",
                errno,
                strerror(errno));
    }

    return( ret );

}

/**
 * Command thread routine.
 */
void *
dz_cmd_run(void *data)
{
    dz_dev_t *dzd = data;
    int ret;

    switch( dzd->cmd_id ) {
    case DZ_CMD_REPORT_ZONES:
	dzd->cmd_do_report_zones = 0;
	ret = dz_report_zones(dzd);
	break;
    case DZ_CMD_RESET_ZONE:
	ret = dz_reset_zone(dzd);
	break;
    case DZ_CMD_OPEN_ZONE:
	ret = dz_open_zone(dzd);
	break;
    case DZ_CMD_CLOSE_ZONE:
	ret = dz_close_zone(dzd);
	break;
    case DZ_CMD_FINISH_ZONE:
	ret = dz_finish_zone(dzd);
	break;
    default:
	fprintf(stderr, "Invalid command ID %d\n", dzd->cmd_id);
	ret = -1;
	break;
    }

    if ( dzd->cmd_do_report_zones ) {
	dz_report_zones(dzd);
    }

    if ( dzd->cmd_dialog ) {
	int response_id;
	if ( ret == 0 ) {
	    response_id = GTK_RESPONSE_OK;
	} else {
	    response_id = GTK_RESPONSE_REJECT;
	}
	gtk_dialog_response(GTK_DIALOG(dzd->cmd_dialog), response_id);
    }

    return( (void *) ((unsigned long) ret) );

}

static GtkWidget *
dz_cmd_dialog(char *msg)
{
    GtkWidget *dialog, *content_area;
    GtkWidget *spinner;

    dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
				    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_OTHER,
				    GTK_BUTTONS_NONE,
				    "%s", msg);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    spinner = gtk_spinner_new();
    gtk_widget_show(spinner);
    gtk_container_add(GTK_CONTAINER(content_area), spinner);
    gtk_spinner_start(GTK_SPINNER(spinner));

    gtk_widget_show_all(dialog);

    return( dialog );

}

/**
 * Open a device.
 */
dz_dev_t *
dz_open(char *path)
{
    dz_dev_t *dzd = NULL;
    int i, ret;

    /* Get an unused device */
    for(i = 0; i < DZ_MAX_DEV; i++) {
	if ( ! dz.dev[i].dev ) {
	    dzd = &dz.dev[i];
	    break;
	}
    }

    if ( ! dzd ) {
	return( NULL );
    }

    /* Open device file */
    strncpy(dzd->path, path, sizeof(dzd->path) - 1);
    ret = zbc_open(dzd->path, O_RDONLY, &dzd->dev);
    if ( ret != 0 ) {
        return( NULL );
    }

    ret = zbc_get_device_info(dzd->dev, &dzd->info);
    if ( ret != 0 ) {
        fprintf(stderr,
                "zbc_get_device_info failed\n");
        goto out;
    }

    dzd->block_size = dz.block_size;
    if ( dzd->block_size ) {
	if ( ((unsigned int) dzd->block_size < dzd->info.zbd_logical_block_size)
	     && (dzd->info.zbd_logical_block_size % dzd->block_size) ) {
	    dzd->block_size = 0;
	} else if ( ((unsigned int) dzd->block_size >= dzd->info.zbd_logical_block_size)
		    && (dzd->block_size % dzd->info.zbd_logical_block_size) ) {
	    dzd->block_size = 0;
	}
    }

    if ( ! dzd->block_size ) {
	dzd->block_size = dzd->info.zbd_logical_block_size;
    }

    /* Get zone information */
    ret = dz_report_zones(dzd);
    if ( ret != 0 ) {
	goto out;
    }

    dz.nr_devs++;

out:

    if ( ret != 0 ) {
	dz_close(dzd);
	dzd = NULL;
    }

    return( dzd );

}

/**
 * Close a device.
 */
void
dz_close(dz_dev_t *dzd)
{

    if ( dzd->dev ) {

	if ( dzd->zbc_zones ) {
	    free(dzd->zbc_zones);
	}

	if ( dzd->zones ) {
	    free(dzd->zones);
	}

	zbc_close(dzd->dev);

	memset(dzd, 0, sizeof(dz_dev_t));
	dz.nr_devs--;

    }

    return;

}

/**
 * Execute a command.
 */
int
dz_cmd_exec(dz_dev_t *dzd,
	    int cmd_id,
	    int do_report_zones,
	    char *msg)
{
    int ret;

    /* Set command */
    dzd->cmd_id = cmd_id;
    dzd->cmd_do_report_zones = do_report_zones;
    if ( msg ) {
	/* Create a dialog */
	dzd->cmd_dialog = dz_cmd_dialog(msg);
    } else {
	dzd->cmd_dialog = NULL;
    }

    /* Create command thread */
    ret = pthread_create(&dzd->cmd_thread, NULL, dz_cmd_run, dzd);
    if ( ret != 0 ) {
	goto out;
    }

    if ( dzd->cmd_dialog ) {
	if ( gtk_dialog_run(GTK_DIALOG(dzd->cmd_dialog)) == GTK_RESPONSE_OK ) {
	    ret = 0;
	} else {
	    ret = -1;
	}
    } else {
	void *cmd_ret;
	pthread_join(dzd->cmd_thread, &cmd_ret);
	ret = (long) cmd_ret;
    }

    pthread_join(dzd->cmd_thread, NULL);

out:

    if ( dzd->cmd_dialog ) {
	gtk_widget_destroy(dzd->cmd_dialog);
	dzd->cmd_dialog = NULL;
    }

    return( ret );

}

