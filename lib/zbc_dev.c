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

/***** Declaration of private funtions *****/

static int
zbc_dev_get_model(zbc_device_t *dev,
                  enum zbc_dev_model *model);

static int
zbc_dev_get_info(zbc_device_t *dev);

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
 * Open and check a device file for ZBC access.
 */
int
zbc_dev_open(zbc_device_t *dev)
{
    enum zbc_dev_model model;
    struct stat st;
    int ret;

    /* Check device */
    ret = stat(dev->zbd_filename, &st);
    if ( ret != 0 ) {
        ret = -errno;
        zbc_error("Stat device %s failed %d (%s)\n",
                  dev->zbd_filename,
                  errno,
                  strerror(errno));
        goto out;
    }

    if ( S_ISBLK(st.st_mode)
         || S_ISREG(st.st_mode) ) {
        dev->zbd_flags |= O_DIRECT;
    } else if ( ! S_ISCHR(st.st_mode) ) {
        zbc_error("File %s is not a supported file type\n",
                  dev->zbd_filename);
        ret = -ENXIO;
        goto out;
    }

    /* Open the device file */
    dev->zbd_fd = open(dev->zbd_filename, dev->zbd_flags);
    if ( dev->zbd_fd < 0 ) {
        ret = -errno;
        zbc_error("Open device file %s failed %d (%s)\n",
                  dev->zbd_filename,
                  errno,
                  strerror(errno));
        goto out;
    }

    /* Set device operation */
    if ( S_ISCHR(st.st_mode) ) {

        /* Assume SG node (this may be a SCSI or SATA device) */
        dev->zbd_ops = &zbc_scsi_ops;

    } else if ( S_ISBLK(st.st_mode) ) {

        /* Regular block device or emulated ZBC device */
        /* on top of a raw regulare block device.      */
        ret = zbc_dev_get_model(dev, &model);
        if ( ret != 0 ) {
            goto out;
        }

        if ( model == ZBC_DM_STANDARD ) {
            /* Emulated device */
            dev->zbd_ops = &zbc_file_ops;
        } else if ( model == ZBC_DM_HOST_MANAGED ) {
            /* ZBC device with regular block device operations */
            dev->zbd_ops = &zbc_blk_ops;
        } else {
             zbc_error("Device %s is not a supported device model\n",
                       dev->zbd_filename);
             ret = -ENXIO;
             goto out;
        }

    } else if ( S_ISREG(st.st_mode) ) {

        /* Emulated device on top of a file */
        dev->zbd_ops = &zbc_file_ops;

    } else {

        /* Unsupported file type */
        ret = -ENXIO;
        goto out;

    }

    /* Get sector size, count, ... */
    ret = zbc_dev_get_info(dev);
    if ( ret != 0 ) {
        zbc_error("Device %s: get device information failed\n",
                  dev->zbd_filename);
    }

out:

    if ( ret != 0 ) {
        if ( dev->zbd_fd >= 0 ) {
            close(dev->zbd_fd);
        }
    }

    return( ret );

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

/**
 * Get the device model.
 */
static int
zbc_dev_get_model(zbc_device_t *dev,
                  enum zbc_dev_model *model)
{
    int dev_type = -1;
    uint8_t *buf = NULL;
    int ret;

    /* INQUIRY device */
    ret = zbc_scsi_inquiry(dev, &buf, &dev_type);
    if ( ret == 0 ) {

        if ( dev_type == ZBC_DEV_TYPE_STANDARD ) {

            /* Standard block device. */
            /* Note: distinction with ZBC_DEV_TYPE_HOST_AWARE needs to be made here... */
            *model = ZBC_DM_STANDARD;

        } else if ( dev_type == ZBC_DEV_TYPE_HOST_MANAGED ) {
            
            /* ZBC host-managed drive */
            *model = ZBC_DM_HOST_MANAGED;
            
        } else {

            /* Unsupported device type */
            *model = -1;

        }

        free(buf);

    }

    return( ret );

}

/**
 * Get a device information (type, model, capacity & sector sizes).
 */
static int
zbc_dev_get_info(zbc_device_t *dev)
{
    return( (dev->zbd_ops->zbd_get_info)(dev) );
}
