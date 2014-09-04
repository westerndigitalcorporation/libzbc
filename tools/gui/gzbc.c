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

#include "gzbc.h"

/***** Local data and functions *****/

/**
 * Demo control.
 */
dz_t dz;

/**
 * Signal handling initialization.
 */
static void
dz_set_signal_handlers(void);

/***** Main *****/

int main(int argc,
         char **argv)
{
    char *filename = NULL;
    gboolean init_ret;
    gboolean verbose = FALSE;
    GError *error = NULL;
    int ret;
    GOptionEntry options[] = {
        {
            "dev", 'd', 0,
            G_OPTION_ARG_FILENAME, &filename,
            "ZBC device file", NULL
        },
        {
            "interval", 'i', 0,
            G_OPTION_ARG_INT, &dz.interval,
            "Refresh interval (milliseconds)", NULL
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
    dz.interval = DZ_INTERVAL;
    gtk_set_locale();
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

    if ( ! filename ) {
        fprintf(stderr, "No ZBC device file specified\n");
        return( 1 );
    }

    if ( verbose ) {
        zbc_set_log_level("debug");
    }

    dz_set_signal_handlers();

    /* Open device file */
    ret = zbc_open(filename, O_RDONLY, &dz.dev);
    if ( ret != 0 ) {
        fprintf(stderr, "Open device %s failed\n",
                filename);
        return( 1 );
    }

    ret = zbc_get_device_info(dz.dev, &dz.info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_get_device_info failed\n");
        goto out;
    }

    dz.filename = filename;

    printf("Creating interface...\n");

    /* Create GUI */
    dz_if_create();

    /* Main event loop */
    gtk_main();
        
    /* Cleanup GUI */
    dz_if_destroy();

out:
    
    zbc_close(dz.dev);

    return( ret );

}

int
dz_get_zones(void)
{
    unsigned int nr_zones;
    int ret;

    ret = zbc_report_nr_zones(dz.dev, 0, ZBC_RO_ALL, &nr_zones);
    if ( ret != 0 ) {
        return( ret );
    }

    if ( dz.nr_zones
         && (dz.nr_zones != nr_zones) ) {
        /* Zone list changed: clean it */
        free(dz.zones);
        dz.zones = NULL;
        dz.nr_zones = 0;
    }

    if ( ! dz.nr_zones ) {
        printf("Device \"%s\": %llu sectors of %u B, %d zones\n",
               dz.filename,
               (unsigned long long) dz.info.zbd_physical_blocks,
               (unsigned int) dz.info.zbd_physical_block_size,
               nr_zones);
    }

    /* Get/refresh zone list */
    ret = zbc_list_zones(dz.dev, 0, ZBC_RO_ALL, &dz.zones, &dz.nr_zones);
    if ( ret != 0 ) {
        ret = errno;
        fprintf(stderr, "zbc_list_zones failed %d (%s)\n",
                errno,
                strerror(errno));
        if ( dz.zones ) {
            free(dz.zones);
            dz.zones = NULL;
            dz.nr_zones = 0;
        }
        goto out;
    }

out:

    return( ret );

}

int
dz_reset_zone(int idx)
{
    int ret = 0;

    if ( (idx >= 0)
         && (idx < dz.nr_zones) ) {
        ret = zbc_reset_write_pointer(dz.dev, dz.zones[idx].zbz_write_pointer);
        if ( ret != 0 ) {
            ret = errno;
            fprintf(stderr, "zbc_reset_write_pointer failed %d (%s)\n",
                    errno,
                    strerror(errno));
        }
    }

    return( ret );

}

/***** Private functions *****/

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

    /* Send signal */
    printf("\nSignal %d caught\n", sig);
    write(dz.sig_pipe[1], &sig, sizeof(int));

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
