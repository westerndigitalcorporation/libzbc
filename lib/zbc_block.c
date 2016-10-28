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
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>

#include "zbc.h"
#include "zbc_sg.h"

#ifdef HAVE_LINUX_BLKZONED_H
#include <linux/blkzoned.h>
#endif

/***** Inline functions *****/

static inline uint64_t zbc_block_lba2bytes(struct zbc_device *dev,
					   uint64_t lba)
{
    return lba * dev->zbd_info.zbd_logical_block_size;
}

static inline uint64_t zbc_block_bytes2lba(struct zbc_device *dev,
					   uint64_t bytes)
{
    return bytes / dev->zbd_info.zbd_logical_block_size;
}

static inline uint64_t zbc_block_lba2sector(struct zbc_device *dev,
					    uint64_t lba)
{
    return zbc_block_lba2bytes(dev, lba) >> 9;
}

static inline uint64_t zbc_block_sector2lba(struct zbc_device *dev,
					    uint64_t sector)
{
    return zbc_block_bytes2lba(dev, sector << 9);
}

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
 * Test if the block device is zoned.
 */
static int
zbc_block_device_is_zoned(struct zbc_device *dev)
{
    char str[128];
    FILE *file;

    /* Check that this is a zoned block device */
    snprintf(str, sizeof(str),
	     "/sys/block/%s/queue/zoned",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	memset(str, 0, sizeof(str));
	fscanf(file, "%s", str);
    }
    fclose(file);

    if ( strcmp(str, "host-aware") == 0 ) {
	dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
	return 1;
    } else if ( strcmp(str, "host-managed") == 0 ) {
	dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
	return 1;
    }

    return 0;

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

    return len;

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
	if ( len )
	    n = snprintf(dev->zbd_info.zbd_vendor_id,
			 ZBC_DEVICE_INFO_LENGTH,
			 "%s ", str);
	fclose(file);
    }

    snprintf(str, sizeof(str),
	     "/sys/block/%s/device/model",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	len = zbc_block_get_str(file, str);
	if ( len )
	    n += snprintf(&dev->zbd_info.zbd_vendor_id[n],
			  ZBC_DEVICE_INFO_LENGTH - n,
			  "%s ", str);
	fclose(file);
    }

    snprintf(str, sizeof(str),
	     "/sys/block/%s/device/rev",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	len = zbc_block_get_str(file, str);
	if ( len )
	    snprintf(&dev->zbd_info.zbd_vendor_id[n],
		     ZBC_DEVICE_INFO_LENGTH - n,
		     "%s", str);
	fclose(file);
	ret = 1;
    }

    return ret;

}

/**
 * Test if the device can be handled
 * and get the block device info.
 */
static int
zbc_block_get_info(struct zbc_device *dev)
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
    if ( ! zbc_block_device_is_zoned(dev) )
	/* Not a zoned block device: ignore */
	return -ENXIO;

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
    if ( zbc_scsi_get_zbd_chars(dev) )
	return -ENXIO;

    /* Get maximum command size */
    zbc_sg_get_max_cmd_blocks(dev);

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
    struct zbc_device *dev;
    int fd, ret;

    zbc_debug("%s: ########## Trying BLOCK driver ##########\n",
	      filename);

#ifndef HAVE_LINUX_BLKZONED_H
    zbc_debug("libzbc compiled without block driver support\n");
    return -ENXIO;
#endif

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
    dev = calloc(1, sizeof(struct zbc_device));
    if ( ! dev )
        goto out;

    dev->zbd_fd = fd;
    dev->zbd_filename = strdup(filename);
    if ( ! dev->zbd_filename )
        goto out_free_dev;

    /* Get device information */
    ret = zbc_block_get_info(dev);
    if ( ret != 0 )
        goto out_free_filename;

    *pdev = dev;

    zbc_debug("%s: ########## BLOCK driver succeeded ##########\n",
	      filename);

    return 0;

out_free_filename:

    free(dev->zbd_filename);

out_free_dev:

    free(dev);

out:

    if ( fd >= 0 )
	close(fd);

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
    int ret = 0;

    /* Close device */
    if ( close(dev->zbd_fd) < 0 )
        ret = -errno;

    if ( ret == 0 ) {
        free(dev->zbd_filename);
        free(dev);
    }

    return ret;

}

#ifdef HAVE_LINUX_BLKZONED_H

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
 * Test if a zone should be reported depending
 * on the specified reporting options.
 */
static bool
zbc_block_must_report(struct zbc_zone *zone,
		      enum zbc_reporting_options ro)
{
    enum zbc_reporting_options options = ro & (~ZBC_RO_PARTIAL);

    switch ( options ) {
    case ZBC_RO_ALL:
        return true;
    case ZBC_RO_EMPTY:
        return zbc_zone_empty(zone);
    case ZBC_RO_IMP_OPEN:
        return zbc_zone_imp_open(zone);
    case ZBC_RO_EXP_OPEN:
        return zbc_zone_exp_open(zone);
    case ZBC_RO_CLOSED:
        return zbc_zone_closed(zone);
    case ZBC_RO_FULL:
        return zbc_zone_full(zone);
    case ZBC_RO_RDONLY:
        return zbc_zone_rdonly(zone);
    case ZBC_RO_OFFLINE:
        return zbc_zone_offline(zone);
    case ZBC_RO_RESET:
        return zbc_zone_need_reset(zone);
    case ZBC_RO_NON_SEQ:
        return zbc_zone_non_seq(zone);
    case ZBC_RO_NOT_WP:
        return zbc_zone_not_wp(zone);
    default:
	break;
    }

    return false;


}

#define ZBC_BLOCK_ZONE_REPORT_NR_ZONES	8192

/**
 * Get the block device zone information.
 */
static int
zbc_block_report_zones(struct zbc_device *dev,
		       uint64_t start_lba,
		       enum zbc_reporting_options ro,
		       uint64_t *max_lba,
		       struct zbc_zone *zones,
		       unsigned int *nr_zones)
{
    struct zbc_zone zone;
    struct blk_zone_report *rep;
    struct blk_zone *blkz;
    unsigned int i, n = 0;
    int ret;

    rep = malloc(sizeof(struct blk_zone_report) +
		 sizeof(struct blk_zone) * ZBC_BLOCK_ZONE_REPORT_NR_ZONES);
    if ( ! rep ) {
 	zbc_error("%s: No memory for report zones\n",
                  dev->zbd_filename);
        return -ENOMEM;
    }
    blkz = (struct blk_zone *)(rep + 1);

    while ( ((! *nr_zones) || (n < *nr_zones))
	    && (start_lba < dev->zbd_info.zbd_logical_blocks) ) {

	/* Get zone info */
	memset(rep, 0, sizeof(struct blk_zone_report) +
	       sizeof(struct blk_zone) * ZBC_BLOCK_ZONE_REPORT_NR_ZONES);
    	rep->sector = zbc_block_lba2sector(dev, start_lba);
    	rep->nr_zones = ZBC_BLOCK_ZONE_REPORT_NR_ZONES;

	ret = ioctl(dev->zbd_fd, BLKREPORTZONE, rep);
	if ( ret != 0 ) {
	    ret = -errno;
	    zbc_error("%s: ioctl BLKREPORTZONE at %llu failed %d (%s)\n",
		      dev->zbd_filename,
		      (unsigned long long)start_lba,
		      errno,
		      strerror(errno));
	    goto out;
	}

   	for(i = 0; i < rep->nr_zones; i++) {

    	    if ( (*nr_zones && (n >= *nr_zones))
	         || (start_lba >= dev->zbd_info.zbd_logical_blocks) )
		break;

	    memset(&zone, 0, sizeof(struct zbc_zone));
	    zone.zbz_type = blkz[i].type;
	    zone.zbz_condition = blkz[i].cond;
	    zone.zbz_length = zbc_block_sector2lba(dev, blkz[i].len);
	    zone.zbz_start = zbc_block_sector2lba(dev, blkz[i].start);
	    zone.zbz_write_pointer = zbc_block_sector2lba(dev, blkz[i].wp);
	    if ( blkz[i].reset )
	        zone.zbz_flags |= ZBC_ZF_NEED_RESET;
	    if ( blkz[i].non_seq )
	        zone.zbz_flags |= ZBC_ZF_NON_SEQ;

	    if ( zbc_block_must_report(&zone, ro) ) {
	        if ( zones )
		    memcpy(&zones[n], &zone, sizeof(struct zbc_zone));
	        n++;
	    }

	    start_lba += zbc_zone_length(&zone);

        }

    }

    /* Return number of zones */
    *nr_zones = n;

out:
    free(rep);

    return ret;

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
 * Reset a single zone write pointer.
 */
static int
zbc_block_reset_one(struct zbc_device *dev,
		    uint64_t start_lba)
{
    struct blk_zone_range range;
    struct zbc_zone zone;
    unsigned int nr_zones = 1;
    int ret;

    /* Get zone info */
    ret = zbc_block_report_zones(dev, start_lba,
				 ZBC_RO_ALL, NULL,
				 &zone, &nr_zones);
    if (ret)
	    return ret;
    if (! nr_zones ) {
	zbc_error("%s: Invalid LBA\n",
		  dev->zbd_filename);
	return -EINVAL;
    }

    if (zbc_zone_conventional(&zone)
	|| zbc_zone_empty(&zone))
	    return 0;

    /* Reset zone */
    range.sector = zbc_block_lba2sector(dev, zbc_zone_start_lba(&zone));
    range.nr_sectors = zbc_block_lba2sector(dev, zbc_zone_length(&zone));
    ret = ioctl(dev->zbd_fd, BLKRESETZONE, &range);
    if ( ret != 0 ) {
	ret = -errno;
	zbc_error("%s: ioctl BLKRESETZONE failed %d (%s)\n",
		  dev->zbd_filename,
		  errno,
		  strerror(errno));
    }

    return ret;

}

/**
 * Reset all zones write pointer.
 */
static int
zbc_block_reset_all(struct zbc_device *dev)
{
    struct zbc_zone *zones;
    unsigned int i, nr_zones;
    struct blk_zone_range range;
    uint64_t start_lba = 0;
    int ret;

    zones = malloc(sizeof(struct zbc_zone) * ZBC_BLOCK_ZONE_REPORT_NR_ZONES);
    if ( ! zones ) {
 	zbc_error("%s: No memory for report zones\n",
                  dev->zbd_filename);
        return -ENOMEM;
    }

    while ( 1 ) {

	/* Get zone info */
	nr_zones = ZBC_BLOCK_ZONE_REPORT_NR_ZONES;
	ret = zbc_block_report_zones(dev, start_lba,
				     ZBC_RO_ALL, NULL,
				     zones, &nr_zones);
	if (ret || (! nr_zones))
	    break;

	for (i = 0; i < nr_zones; i++) {

	    start_lba = zbc_zone_next_lba(&zones[i]);

	    if (zbc_zone_conventional(&zones[i])
		|| zbc_zone_empty(&zones[i]))
		continue;

	    /* Reset zone */
	    range.sector = zbc_block_lba2sector(dev, zbc_zone_start_lba(&zones[i]));
	    range.nr_sectors = zbc_block_lba2sector(dev, zbc_zone_length(&zones[i]));
	    ret = ioctl(dev->zbd_fd, BLKRESETZONE, &range);
	    if ( ret != 0 ) {
		ret = -errno;
		zbc_error("%s: ioctl BLKRESETZONE failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		goto out;
	    }

	}

    }

out:
    free(zones);

    return ret;

}

/**
 * Reset zone(s) write pointer: use BLKDISCARD ioctl.
 */
static int
zbc_block_reset_wp(struct zbc_device *dev,
		   uint64_t start_lba)
{

    if ( start_lba == (uint64_t)-1 )
	/* All zones */
	return zbc_block_reset_all(dev);

    /* One zone */
    return zbc_block_reset_one(dev, start_lba);

}

/**
 * Read from the block device.
 */
static int32_t
zbc_block_pread(struct zbc_device *dev,
		zbc_zone_t *zone,
		void *buf,
		uint32_t lba_count,
		uint64_t lba_ofst)
{
    ssize_t ret;

    /* Read */
    ret = pread(dev->zbd_fd,
		buf,
		zbc_block_lba2bytes(dev, lba_count),
		zbc_block_lba2bytes(dev, zone->zbz_start + lba_ofst));
    if ( ret < 0 )
        ret = -errno;
    else
        ret = zbc_block_bytes2lba(dev, ret);

    return ret;

}

/**
 * Write to the block device.
 */
static int32_t
zbc_block_pwrite(struct zbc_device *dev,
		 zbc_zone_t *zone,
		 const void *buf,
		 uint32_t lba_count,
		 uint64_t lba_ofst)
{
    ssize_t ret;

    /* Read */
    ret = pwrite(dev->zbd_fd,
		 buf,
		 zbc_block_lba2bytes(dev, lba_count),
		 zbc_block_lba2bytes(dev, zone->zbz_start + lba_ofst));
    if ( ret < 0 )
        ret = -errno;
    else
        ret = zbc_block_bytes2lba(dev, ret);

    return ret;

}

#else /* HAVE_LINUX_BLKZONED_H */

static int
zbc_block_report_zones(struct zbc_device *dev,
		       uint64_t start_lba,
		       enum zbc_reporting_options ro,
		       uint64_t *max_lba,
		       struct zbc_zone *zones,
		       unsigned int *nr_zones)
{
    return -EOPNOTSUPP;
}

static int
zbc_block_open_zone(zbc_device_t *dev,
                    uint64_t start_lba)
{
    return -EOPNOTSUPP;
}

static int
zbc_block_close_zone(zbc_device_t *dev,
                     uint64_t start_lba)
{
    return -EOPNOTSUPP;
}

static int
zbc_block_finish_zone(zbc_device_t *dev,
                      uint64_t start_lba)
{
    return -EOPNOTSUPP;
}


static int
zbc_block_reset_wp(struct zbc_device *dev,
		   uint64_t start_lba)
{
    return -EOPNOTSUPP;
}

static int32_t
zbc_block_pread(struct zbc_device *dev,
		zbc_zone_t *zone,
		void *buf,
		uint32_t lba_count,
		uint64_t lba_ofst)
{
    return -EOPNOTSUPP;
}

static int32_t
zbc_block_pwrite(struct zbc_device *dev,
		 zbc_zone_t *zone,
		 const void *buf,
		 uint32_t lba_count,
		 uint64_t lba_ofst)
{
    return -EOPNOTSUPP;
}

static int
zbc_block_flush(struct zbc_device *dev,
		uint64_t lba_offset,
		uint32_t lba_count,
		int immediate)
{
    return -EOPNOTSUPP;
}

#endif /* HAVE_LINUX_BLKZONED_H */

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
