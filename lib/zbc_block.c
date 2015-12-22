/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc.  All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Author: Christoph Hellwig (hch@infradead.org)
 *         Damien Le Moal (damien.lemoal@hgst.com)
 */

/***** Including files *****/

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>

#include "zbc.h"

/***** Macro and types definitions *****/

/**
 * Block device descriptor data.
 */
typedef struct zbc_block_device {

    struct zbc_device   dev;

    unsigned int        zone_sectors;

} zbc_block_device_t;

/***** Definition of private functions *****/

/**
 * Convert device logical block size value into 512 B sector value.
 */
static inline uint64_t
zbc_block_lba_to_sector(struct zbc_device *dev,
			uint64_t val)
{
    return (val * dev->zbd_info.zbd_logical_block_size) >> 9;
}

/**
 * Convert device address to block device handle address.
 */
static inline zbc_block_device_t *
zbc_dev_to_block_dev(struct zbc_device *dev)
{
    return container_of(dev, struct zbc_block_device, dev);
}

/**
 * Test if the block device is zoned.
 */
static int
zbc_block_device_is_zoned(struct zbc_device *dev)
{
    zbc_block_device_t *bdev = zbc_dev_to_block_dev(dev);
    unsigned long long start, len;
    unsigned int type, nr_zones;
    char str[128];
    FILE *zoned;
    int ret = 1;

    /* Check zoned attributes, if any */
    snprintf(str, sizeof(str),
	     "/sys/block/%s/queue/zoned",
	     basename(dev->zbd_filename));
    zoned = fopen(str, "r");
    if ( ! zoned ) {
	/* Not a zoned block device or no kernel support */
	return 0;
    }

    while( 1 ) {

	start = len = 0;
	type = -1;
	nr_zones = 0;
	ret = fscanf(zoned, "%llu %llu %u %u", &start, &len, &type, &nr_zones);
	if ( ret == EOF ) {
	    break;
	}

	if ( nr_zones == 0 ) {
	    /* Not a zoned block device */
	    ret = 0;
	    break;
	}

	if ( type == 2 ) {
	    dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
	    bdev->zone_sectors = len;
	} else if ( type == 3 ) {
	    dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
	    bdev->zone_sectors = len;
	}

    }

    fclose(zoned);

    return ret;

}

/**
 * Get a string in a file and strip it of trailing
 * spaces and carriage return.
 */
static int
zbc_block_get_str(FILE *file,
		  char *str)
{
    int len = 0;

    if ( fgets(str, 128, file) ) {
	len = strlen(str) - 1;
	while( len > 0 ) {
	    if ( (str[len] == ' ')
		 || (str[len] == '\t')
		 || (str[len] == '\r')
		 || (str[len] == '\n') ) {
		str[len] = '\0';
		len--;
	    } else {
		break;
	    }
	}
    }

    return( len );

}

/**
 * Get vendor ID.
 */
static int
zbc_block_get_vendor_id(struct zbc_device *dev)
{
    char str[128];
    FILE *file;
    int n = 0, len, ret = 0;

    snprintf(str, sizeof(str),
	     "/sys/block/%s/device/vendor",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	len = zbc_block_get_str(file, str);
	if ( len ) {
	    n = snprintf(dev->zbd_info.zbd_vendor_id,
			 ZBC_DEVICE_INFO_LENGTH,
			 "%s ", str);
	}
	fclose(file);
    }

    snprintf(str, sizeof(str),
	     "/sys/block/%s/device/model",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	len = zbc_block_get_str(file, str);
	if ( len ) {
	    n += snprintf(&dev->zbd_info.zbd_vendor_id[n],
			  ZBC_DEVICE_INFO_LENGTH - n,
			  "%s ", str);
	}
	fclose(file);
    }

    snprintf(str, sizeof(str),
	     "/sys/block/%s/device/rev",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	len = zbc_block_get_str(file, str);
	if ( len ) {
	    snprintf(&dev->zbd_info.zbd_vendor_id[n],
		     ZBC_DEVICE_INFO_LENGTH - n,
		     "%s", str);
	}
	fclose(file);
	ret = 1;
    }

    return ret;

}

/**
 * Test if the device can be handled
 * and set a the block device info.
 */
static int
zbc_block_set_info(struct zbc_device *dev)
{
    unsigned long long size64;
    struct stat st;
    int size32;
    int ret;

    /* Get device stats */
    if ( fstat(dev->zbd_fd, &st) < 0 ) {
        ret = -errno;
        zbc_error("%s: stat failed %d (%s)\n",
                  dev->zbd_filename,
                  errno,
                  strerror(errno));
        return ret;
    }

    if ( ! S_ISBLK(st.st_mode) ) {
	/* Not a block device: ignore */
	return -ENXIO;
    }

    /* Is this a zoned device ? And do we have kernel support ? */
    if ( ! zbc_block_device_is_zoned(dev) ) {
	/* Not a zoned block device: ignore */
	return -ENXIO;
    }

    /* Get logical block size */
    ret = ioctl(dev->zbd_fd, BLKSSZGET, &size32);
    if ( ret != 0 ) {
	ret = -errno;
	zbc_error("%s: ioctl BLKSSZGET failed %d (%s)\n",
		  dev->zbd_filename,
		  errno,
		  strerror(errno));
	return ret;
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
	return ret;
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
	return ret;
    }

    if ( dev->zbd_info.zbd_logical_block_size <= 0 ) {
	zbc_error("%s: invalid logical sector size %d\n",
		  dev->zbd_filename,
		  size32);
	return -EINVAL;
    }
    dev->zbd_info.zbd_logical_blocks = size64 / dev->zbd_info.zbd_logical_block_size;

    if ( dev->zbd_info.zbd_physical_block_size <= 0 ) {
	zbc_error("%s: invalid physical sector size %d\n",
		  dev->zbd_filename,
		  size32);
	return -EINVAL;
    }
    dev->zbd_info.zbd_physical_blocks = size64 / dev->zbd_info.zbd_physical_block_size;

    /* Check */
    if ( ! dev->zbd_info.zbd_logical_blocks ) {
        zbc_error("%s: invalid capacity (logical blocks)\n",
                  dev->zbd_filename);
        return -EINVAL;
    }

    if ( ! dev->zbd_info.zbd_physical_blocks ) {
        zbc_error("%s: invalid capacity (physical blocks)\n",
                  dev->zbd_filename);
        return -EINVAL;
    }

    /* Finish setting */
    dev->zbd_info.zbd_type = ZBC_DT_BLOCK;
    if ( ! zbc_block_get_vendor_id(dev) ) {
	strncpy(dev->zbd_info.zbd_vendor_id, "Unknown", ZBC_DEVICE_INFO_LENGTH - 1);
    }

    /* Use SG_IO to get zone characteristics (maximum number of open zones, etc) */
    if ( zbc_scsi_get_zbd_chars(dev) ) {
	return -ENXIO;
    }

    return 0;

}

/**
 * Open a block device.
 */
static int
zbc_block_open(const char *filename,
	       int flags,
	       struct zbc_device **pdev)
{
    zbc_block_device_t *bdev;
    struct zbc_device *dev;
    int fd, ret;

    zbc_debug("%s: ########## Trying BLOCK driver ##########\n",
	      filename);

    /* Open block device: always add write mode for discard (reset zone) */
    fd = open(filename, zbc_open_flags(flags) | O_WRONLY);
    if ( fd < 0 ) {
        zbc_error("%s: open failed %d (%s)\n",
                  filename,
                  errno,
                  strerror(errno));
        ret = -errno;
	goto out;
    }

    /* Allocate a handle */
    ret = -ENOMEM;
    bdev = calloc(1, sizeof(*bdev));
    if ( ! bdev ) {
        goto out;
    }

    bdev->zone_sectors = 0;
    dev = &bdev->dev;
    dev->zbd_fd = fd;
    dev->zbd_filename = strdup(filename);
    if ( ! dev->zbd_filename ) {
        goto out_free_dev;
    }

    /* Set the fake device information */
    ret = zbc_block_set_info(dev);
    if ( ret != 0 ) {
        goto out_free_filename;
    }

    *pdev = dev;

    zbc_debug("%s: ########## BLOCK driver succeeded ##########\n",
	      filename);

    return 0;

out_free_filename:

    free(dev->zbd_filename);

out_free_dev:

    free(bdev);

out:

    if ( fd >= 0 ) {
	close(fd);
    }

    zbc_debug("%s: ########## BLOCK driver failed %d ##########\n",
	      filename,
	      ret);

    return ret;

}

/**
 * Close a device.
 */
static int
zbc_block_close(zbc_device_t *dev)
{
    zbc_block_device_t *bdev = zbc_dev_to_block_dev(dev);
    int ret = 0;

    /* Close device */
    if ( close(dev->zbd_fd) < 0 ) {
        ret = -errno;
    }

    if ( ret == 0 ) {
        free(dev->zbd_filename);
        free(bdev);
    }

    return ret;

}

/**
 * Flush the device.
 */
static int
zbc_block_flush(struct zbc_device *dev,
		uint64_t lba_offset,
		uint32_t lba_count,
		int immediate)
{
    return fsync(dev->zbd_fd);
}

/**
 * Get the block device zone information: use SG_IO, but
 * sync the device first to ensure that the current write
 * pointer value is returned.
 */
static int
zbc_block_report_zones(struct zbc_device *dev,
		       uint64_t start_lba,
		       enum zbc_reporting_options ro,
		       uint64_t *max_lba,
		       struct zbc_zone *zones,
		       unsigned int *nr_zones)
{

    fdatasync(dev->zbd_fd);

    return zbc_scsi_report_zones(dev, start_lba, ro,
				 max_lba, zones, nr_zones);

}

/**
 * Open zone(s): use SG_IO.
 */
static int
zbc_block_open_zone(zbc_device_t *dev,
		    uint64_t start_lba)
{
    return zbc_scsi_open_zone(dev, start_lba);
}

/**
 * Close zone(s): use SG_IO.
 */
static int
zbc_block_close_zone(zbc_device_t *dev,
		     uint64_t start_lba)
{
    return zbc_scsi_close_zone(dev, start_lba);
}

/**
 * Finish zone(s): use SG_IO.
 */
static int
zbc_block_finish_zone(zbc_device_t *dev,
		      uint64_t start_lba)
{
    return zbc_scsi_finish_zone(dev, start_lba);
}

/**
 * Reset zone(s) write pointer: use BLKDISCARD ioctl.
 */
static int
zbc_block_reset_wp(struct zbc_device *dev,
		   uint64_t start_lba)
{
    zbc_block_device_t *bdev = zbc_dev_to_block_dev(dev);
    uint64_t range[2];

    if ( start_lba == (uint64_t)-1 ) {
        /* Reset all zones */
	range[0] = 0;
	range[1] = dev->zbd_info.zbd_logical_blocks * dev->zbd_info.zbd_logical_block_size;
    } else {
        /* Reset only the zone at start_lba */
	range[0] = start_lba * dev->zbd_info.zbd_logical_block_size;
	range[1] = bdev->zone_sectors << 9;
    }

    /* Discard */
    if ( ioctl(dev->zbd_fd, BLKDISCARD, &range) != 0 ) {
	return -errno;
    }

    return 0;

}

/**
 * Read from the block device.
 */
static int32_t
zbc_block_pread(struct zbc_device *dev,
		struct zbc_zone *z,
		void *buf,
		uint32_t lba_count,
		uint64_t start_lba)
{
    ssize_t ret;

    /* Read */
    ret = pread(dev->zbd_fd,
		buf,
		lba_count * dev->zbd_info.zbd_logical_block_size,
		start_lba * dev->zbd_info.zbd_logical_block_size);
    if ( ret < 0 ) {
        ret = -errno;
    } else {
        ret /= dev->zbd_info.zbd_logical_block_size;
    }

    return ret;

}

/**
 * Write to the block device.
 */
static int32_t
zbc_block_pwrite(struct zbc_device *dev,
		 struct zbc_zone *z,
		 const void *buf,
		 uint32_t lba_count,
		 uint64_t start_lba)
{
    ssize_t ret;

    /* Read */
    ret = pwrite(dev->zbd_fd,
		 buf,
		 lba_count * dev->zbd_info.zbd_logical_block_size,
		 start_lba * dev->zbd_info.zbd_logical_block_size);
    if ( ret < 0 ) {
        ret = -errno;
    } else {
        ret /= dev->zbd_info.zbd_logical_block_size;
    }

    return ret;

}

struct zbc_ops zbc_block_ops = {
    .zbd_open         = zbc_block_open,
    .zbd_close        = zbc_block_close,
    .zbd_pread        = zbc_block_pread,
    .zbd_pwrite       = zbc_block_pwrite,
    .zbd_flush        = zbc_block_flush,
    .zbd_report_zones = zbc_block_report_zones,
    .zbd_open_zone    = zbc_block_open_zone,
    .zbd_close_zone   = zbc_block_close_zone,
    .zbd_finish_zone  = zbc_block_finish_zone,
    .zbd_reset_wp     = zbc_block_reset_wp,
};
