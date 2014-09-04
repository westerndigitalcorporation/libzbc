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

#include "zbc.h"

/***** Definition of private functions *****/

/**
 * Get device zone information.
 */
static int
zbc_ata_report_zones(zbc_device_t *dev,
                      uint64_t start_lba,
                      enum zbc_reporting_options ro,
                      zbc_zone_t *zones,
                      unsigned int *nr_zones)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}

/**
 * Reset zone(s) write pointer.
 */
static int
zbc_ata_reset_write_pointer(zbc_device_t *dev,
                             uint64_t start_lba)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}

/**
 * Configure zones of a "emulated" ZBC device
 */
static int
zbc_ata_set_zones(zbc_device_t *dev,
                   uint64_t conv_sz,
                   uint64_t seq_sz)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}

/**
 * Change the value of a zone write pointer ("emulated" ZBC devices only).
 */
static int
zbc_ata_set_write_pointer(struct zbc_device *dev,
                           uint64_t start_lba,
                           uint64_t write_pointer)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}

static int
zbc_ata_get_info(zbc_device_t *dev)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );
}

static int32_t
zbc_ata_pread(zbc_device_t *dev,
                 zbc_zone_t *zone,
                 void *buf,
                 uint32_t lba_count,
                 uint64_t lba_ofst)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}

static int32_t
zbc_ata_pwrite(zbc_device_t *dev,
                  zbc_zone_t *zone,
                  const void *buf,
                  uint32_t lba_count,
                  uint64_t lba_ofst)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}

static int
zbc_ata_flush(zbc_device_t *dev,
                 uint64_t lba_ofst,
                 uint32_t lba_count,
                 int immediate)
{

    zbc_error("ZAC drives are not supported for now.\n");

    return( -ENOSYS );

}


/***** Definition of public data *****/

/**
 * ZAC SATA command operations.
 */
zbc_ops_t zbc_ata_ops =
{
    .zbd_get_info     = zbc_ata_get_info,
    .zbd_pread        = zbc_ata_pread,
    .zbd_pwrite       = zbc_ata_pwrite,
    .zbd_flush        = zbc_ata_flush,
    .zbd_report_zones = zbc_ata_report_zones,
    .zbd_reset_wp     = zbc_ata_reset_write_pointer,
    .zbd_set_zones    = zbc_ata_set_zones,
    .zbd_set_wp       = zbc_ata_set_write_pointer,

};

