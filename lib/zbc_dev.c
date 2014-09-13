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
 * Author: Damien Le Moal (damien.lemoal@hgst.com)
 */

/***** Including files *****/

#define _GNU_SOURCE     /* O_DIRECT */

#include "zbc.h"
#include "zbc_sg.h"
#include "zbc_scsi.h" /* XXX: for zbc_scsi_inquiry */

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/hdreg.h>

/***** Definition of public functions *****/

/**
 * Allocate and initialize a device descriptor.
 */
zbc_device_t *
zbc_dev_alloc(const char *filename,
              int flags)
{
    zbc_device_t *dev = malloc(sizeof(struct zbc_device));

    if ( dev ) {

        memset(dev, 0, sizeof(struct zbc_device));
        dev->zbd_meta_fd = -1;
        dev->zbd_flags = flags;
        dev->zbd_filename = strdup(filename);
        if ( ! dev->zbd_filename ) {
            free(dev);
            dev = NULL;
        }

    }

    return( dev );

}

/**
 * Free a device descriptor.
 */
void
zbc_dev_free(zbc_device_t *dev)
{

    if ( dev ) {
        if ( dev->zbd_filename ) {
            free(dev->zbd_filename);
        }
        free(dev);
    }

    return;

}

/**
 * Close a ZBC file handle.
 */
int
zbc_dev_close(zbc_device_t *dev)
{
    int ret;

    if ( dev->zbd_meta_fd != -1 )
        close(dev->zbd_meta_fd);
    if ( dev->zbd_fd >= 0 ) {
        ret = close(dev->zbd_fd);
    } else {
        ret = 0;
    }

    return( ret );

}
