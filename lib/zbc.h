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
 *          Christoph Hellwig (hch@infradead.org)
 */

#ifndef __LIBZBC_INTERNAL_H__
#define __LIBZBC_INTERNAL_H__

/***** Including files *****/

#include <libzbc/zbc.h>
#include "zbc_log.h"

#include <stdlib.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

/***** Type definitions *****/

/**
 * Device operations.
 */
typedef struct zbc_ops {
    /**
     * Open device.
     */
    int         (*zbd_open)(const char *filename, int flags,
                            struct zbc_device **pdev);

    /**
     * Close device.
     */
    int         (*zbd_close)(struct zbc_device *dev);

    /**
     * Read from a ZBC device
     */
    int32_t     (*zbd_pread)(struct zbc_device *,
                             zbc_zone_t *,
                             void *,
                             uint32_t,
                             uint64_t);

    /**
     * Write to a ZBC device
     */
    int32_t     (*zbd_pwrite)(struct zbc_device *,
                              zbc_zone_t *,
                              const void *,
                              uint32_t,
                              uint64_t);

    /**
     * Flush to a ZBC device cache.
     */
    int         (*zbd_flush)(struct zbc_device *,
                             uint64_t,
                             uint32_t,
                             int immediate);

    /**
     * Report a device zone information.
     * (mandatory)
     */
    int         (*zbd_report_zones)(struct zbc_device *,
                                    uint64_t,
                                    enum zbc_reporting_options,
                                    zbc_zone_t *,
                                    unsigned int *);

    /**
     * Reset a zone or all zones write pointer.
     * (mandatory)
     */
    int         (*zbd_reset_wp)(struct zbc_device *,
                                uint64_t);

    /**
     * Change a device zone configuration.
     * For emulated drives only (optional).
     */
    int         (*zbd_set_zones)(struct zbc_device *,
                                 uint64_t,
                                 uint64_t);
    
    /**
     * Change a zone write pointer.
     * For emulated drives only (optional).
     */
    int         (*zbd_set_wp)(struct zbc_device *,
                              uint64_t,
                              uint64_t);

} zbc_ops_t;

/**
 * Device descriptor.
 */
typedef struct zbc_device {

    /**
     * Device file path.
     */
    char                *zbd_filename;

    /**
     * Device info.
     */
    zbc_device_info_t   zbd_info;

    /**
     * Device file descriptor.
     */
    int                 zbd_fd;

    /**
     * Device operations.
     */
    zbc_ops_t           *zbd_ops;

} zbc_device_t;

/***** Internal device functions *****/

/**
 * ZAC (ATA) device operations.
 */
extern zbc_ops_t zbc_ata_ops;

/**
 * ZBC (SCSI) device operations.
 */
extern zbc_ops_t zbc_scsi_ops;

/**
 * ZBC emulation (file or block device).
 */
extern struct zbc_ops zbc_fake_ops;

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#endif /* __LIBZBC_INTERNAL_H__ */
