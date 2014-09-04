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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "zbc.h"
#include "zbc_scsi.h"

/***** Definition of private funtions *****/

/**
 * Get a block device information (capacity & sector sizes).
 */
int
zbc_blkdev_get_info(zbc_device_t *dev)
{
    unsigned long long size64;
    int size32;
    int ret;

    /* Get logical block size */
    ret = ioctl(dev->zbd_fd, BLKSSZGET, &size32);
    if ( ret != 0 ) {
        ret = -errno;
        zbc_error("%s: ioctl BLKSSZGET failed %d (%s)\n",
                  dev->zbd_filename,
                  errno,
                  strerror(errno));
        goto out;
    }
    
    dev->zbd_info.zbd_logical_block_size = size32;
    
    /* Get physical block size */
    ret = ioctl(dev->zbd_fd, BLKPBSZGET, &size32);
    if ( ret != 0 ) {
        ret = -errno;
        zbc_error("%s: ioctl BLKPBSZGET failed %d (%s)\n",
                  dev->zbd_filename,
                  errno,
                  strerror(errno));
        goto out;
    }
    dev->zbd_info.zbd_physical_block_size = size32;
    
    /* Get capacity (B) */
    ret = ioctl(dev->zbd_fd, BLKGETSIZE64, &size64);
    if ( ret != 0 ) {
        ret = -errno;
        zbc_error("%s: ioctl BLKGETSIZE64 failed %d (%s)\n",
                  dev->zbd_filename,
                  errno,
                  strerror(errno));
        goto out;
    }
    
    /* Check */
    if ( dev->zbd_info.zbd_logical_block_size <= 0 ) {
        zbc_error("%s: invalid logical sector size %d\n",
                  dev->zbd_filename,
                  size32);
        ret = -EINVAL;
        goto out;
    }

    dev->zbd_info.zbd_logical_blocks = size64 / dev->zbd_info.zbd_logical_block_size;
    if ( ! dev->zbd_info.zbd_logical_blocks ) {
        zbc_error("%s: invalid capacity (logical blocks)\n",
                  dev->zbd_filename);
        ret = -EINVAL;
        goto out;
    }

    if ( dev->zbd_info.zbd_physical_block_size <= 0 ) {
        zbc_error("%s: invalid physical sector size %d\n",
                  dev->zbd_filename,
                  size32);
        ret = -EINVAL;
        goto out;
    }
        
    dev->zbd_info.zbd_physical_blocks = size64 / dev->zbd_info.zbd_physical_block_size;
    if ( ! dev->zbd_info.zbd_physical_blocks ) {
        zbc_error("%s: invalid capacity (physical blocks)\n",
                  dev->zbd_filename);
        ret = -EINVAL;
    }

out:

    return( ret );

}

/**
 * Read from a ZBC device
 */
static int32_t
zbc_blkdev_pread(zbc_device_t *dev,
                 zbc_zone_t *zone,
                 void *buf,
                 uint32_t lba_count,
                 uint64_t lba_ofst)
{
    size_t sz = (size_t) lba_count * dev->zbd_info.zbd_logical_block_size;
    loff_t ofst = (zone->zbz_start + lba_ofst) * dev->zbd_info.zbd_logical_block_size;
    ssize_t ret;

    ret = pread(dev->zbd_fd, buf, sz, ofst);
    if ( ret <= 0 ) {
        ret = -errno;
        zbc_error("Read %zu B at %llu failed %d (%s)\n",
                  sz,
                  (unsigned long long) ofst,
                  errno,
                  strerror(errno));
    } else {
        ret /= dev->zbd_info.zbd_logical_block_size;
    }

    return( ret );

}

/**
 * Write to a ZBC device
 */
static int32_t
zbc_blkdev_pwrite(zbc_device_t *dev,
                  zbc_zone_t *zone,
                  const void *buf,
                  uint32_t lba_count,
                  uint64_t lba_ofst)
{
    ssize_t ret;
    size_t sz = (size_t) lba_count * dev->zbd_info.zbd_logical_block_size;
    loff_t ofst = (zone->zbz_start + lba_ofst) * dev->zbd_info.zbd_logical_block_size;

    ret = pwrite(dev->zbd_fd, buf, sz, ofst);
    if ( ret <= 0 ) {
        ret = -errno;
        zbc_error("Write %zu B at %llu (sector %llu) failed %d (%s)\n",
                  sz,
                  (unsigned long long) ofst,
                  (unsigned long long) zone->zbz_write_pointer,
                  errno,
                  strerror(errno));
    } else {
        ret /= dev->zbd_info.zbd_logical_block_size;
        zone->zbz_write_pointer += ret;
    }

    return( ret );

}

/**
 * Flush to a ZBC device cache.
 */
static int
zbc_blkdev_flush(zbc_device_t *dev,
                 uint64_t lba_ofst,
                 uint32_t lba_count,
                 int immediate)
{

    return( fsync(dev->zbd_fd) );
    
}

/**
 * ZBC with regular block device I/O operations.
 */
zbc_ops_t zbc_blk_ops =
{
    .zbd_get_info     = zbc_blkdev_get_info,
    .zbd_pread        = zbc_blkdev_pread,
    .zbd_pwrite       = zbc_blkdev_pwrite,
    .zbd_flush        = zbc_blkdev_flush,
    .zbd_report_zones = zbc_scsi_report_zones,
    .zbd_reset_wp     = zbc_scsi_reset_write_pointer,
    .zbd_set_zones    = zbc_scsi_set_zones,
    .zbd_set_wp       = zbc_scsi_set_write_pointer,
};
