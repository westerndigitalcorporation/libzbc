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
#include <linux/blkzoned.h>

#include "zbc.h"
#include "zbc_sg.h"

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
    unsigned int type = -1;
    int is_zoned = 0;
    char str[128];
    FILE *file;

    /* Check that this is a zoned block device */
    snprintf(str, sizeof(str),
	     "/sys/block/%s/queue/zoned",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	fscanf(file, "%d", &is_zoned);
    }
    fclose(file);

    if ( ! is_zoned ) {
	goto out;
    }

    /* Get device type */
    snprintf(str, sizeof(str),
	     "/sys/block/%s/device/type",
	     basename(dev->zbd_filename));
    file = fopen(str, "r");
    if ( file ) {
	fscanf(file, "%u", &type);
    }
    fclose(file);

    if ( type == 0x14 ) {
	dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
    } else if ( type == 0x0 ) {
	dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
    }

out:

    return is_zoned;

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
    if ( ! dev ) {
        goto out;
    }

    dev->zbd_fd = fd;
    dev->zbd_filename = strdup(filename);
    if ( ! dev->zbd_filename ) {
        goto out_free_dev;
    }

    /* Get device information */
    ret = zbc_block_get_info(dev);
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

    free(dev);

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
    int ret = 0;

    /* Close device */
    if ( close(dev->zbd_fd) < 0 ) {
        ret = -errno;
    }

    if ( ret == 0 ) {
        free(dev->zbd_filename);
        free(dev);
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
 * Test if a zone should be reported depending
 * on the specified reporting options.
 */
static bool
zbc_block_must_report(struct zbc_zone *zone,
		      enum zbc_reporting_options ro)
{
    enum zbc_reporting_options options = ro & (~ZBC_RO_PARTIAL);

    switch( options ) {
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
    struct blkzone blkz;
    unsigned int n = 0;
    int ret;

    while( ((! *nr_zones) || (n < *nr_zones))
	   && (start_lba < dev->zbd_info.zbd_logical_blocks) ) {

	/* Get zone info */
	memset(&blkz, 0, sizeof(struct blkzone));
	blkz.start = zbc_block_lba2sector(dev, start_lba);
	ret = ioctl(dev->zbd_fd, BLKREPORTZONE, &blkz);
	if ( ret != 0 ) {
	    ret = -errno;
	    zbc_error("%s: ioctl BLKREPORTZONE failed %d (%s)\n",
		      dev->zbd_filename,
		      errno,
		      strerror(errno));
	    return ret;
	}

	memset(&zone, 0, sizeof(struct zbc_zone));

	/* Zone start, length and write pointer position */
	zone.zbz_length = zbc_block_sector2lba(dev, blkz.len);
	zone.zbz_start = zbc_block_sector2lba(dev, blkz.start);
	zone.zbz_write_pointer = zbc_block_sector2lba(dev, blkz.wp);

	/* Zone type */
	switch( blkz.type ) {
	case BLKZONE_TYPE_CONVENTIONAL:
	    zone.zbz_type = ZBC_ZT_CONVENTIONAL;
	    break;
	case BLKZONE_TYPE_SEQWRITE_REQ:
	    zone.zbz_type = ZBC_ZT_SEQUENTIAL_REQ;
	    break;
	case BLKZONE_TYPE_SEQWRITE_PREF:
	    zone.zbz_type = ZBC_ZT_SEQUENTIAL_PREF;
	    break;
	default:
	    zone.zbz_type = 0;
	    break;
	}

	/* Zone condition */
	switch( blkz.cond ) {
	case BLKZONE_COND_NOT_WP:
	    zone.zbz_condition = ZBC_ZC_NOT_WP;
	    break;
	case BLKZONE_COND_EMPTY:
	    zone.zbz_condition = ZBC_ZC_EMPTY;
	    break;
	case BLKZONE_COND_OPEN:
	    zone.zbz_condition = ZBC_ZC_EXP_OPEN;
	    break;
	case BLKZONE_COND_CLOSED:
	    zone.zbz_condition = ZBC_ZC_CLOSED;
	    break;
	case BLKZONE_COND_READONLY:
	    zone.zbz_condition = ZBC_ZC_RDONLY;
	    break;
	case BLKZONE_COND_FULL:
	    zone.zbz_condition = ZBC_ZC_FULL;
	    break;
	case BLKZONE_COND_OFFLINE:
	    zone.zbz_condition = ZBC_ZC_OFFLINE;
	    break;
	default:
	    /* Unknown */
	    zone.zbz_condition = -1;
	    break;
	}

	/* Zone flags */
	if ( blkz.cond & BLKZONE_FLAG_NEED_RESET ) {
	    zone.zbz_flags |= ZBC_ZF_NEED_RESET;
	}
	if ( blkz.cond & BLKZONE_FLAG_NON_SEQ ) {
	    zone.zbz_flags |= ZBC_ZF_NON_SEQ;
	}

	if ( zbc_block_must_report(&zone, ro) ) {
	    if ( *nr_zones ) {
		memcpy(&zones[n], &zone, sizeof(struct zbc_zone));
	    }
	    n++;
	}

	start_lba += zbc_zone_length(&zone);

    }

    /* Return number of zones */
    *nr_zones = n;

    return ret;

}

/**
 * Open zone(s).
 */
static int
zbc_block_open_zone(zbc_device_t *dev,
		    uint64_t start_lba)
{
    uint64_t sector;
    int ret;

    if ( start_lba == (uint64_t)-1 ) {
	/* For now */
	return -EINVAL;
    }

    /* Open the zone */
    sector = zbc_block_lba2sector(dev, start_lba);
    ret = ioctl(dev->zbd_fd, BLKOPENZONE, &sector);
    if ( ret != 0 ) {
	ret = -errno;
	zbc_error("%s: ioctl BLKOPENZONE failed %d (%s)\n",
		  dev->zbd_filename,
		  errno,
		  strerror(errno));
	return ret;
    }

    return 0;

}

/**
 * Close zone(s): use SG_IO.
 */
static int
zbc_block_close_zone(zbc_device_t *dev,
		     uint64_t start_lba)
{
    uint64_t sector;
    int ret;

    if ( start_lba == (uint64_t)-1 ) {
	/* For now */
	return -EINVAL;
    }

    /* CLose the zone */
    sector = zbc_block_lba2sector(dev, start_lba);
    ret = ioctl(dev->zbd_fd, BLKCLOSEZONE, &sector);
    if ( ret != 0 ) {
	ret = -errno;
	zbc_error("%s: ioctl BLKCLOSEZONE failed %d (%s)\n",
		  dev->zbd_filename,
		  errno,
		  strerror(errno));
	return ret;
    }

    return 0;

}

/**
 * Finish zone(s): use SG_IO.
 */
static int
zbc_block_finish_zone(zbc_device_t *dev,
		      uint64_t start_lba)
{
    uint64_t sector;
    int ret;

    if ( start_lba == (uint64_t)-1 ) {
	/* For now */
	return -EINVAL;
    }

    /* Finish the zone */
    sector = zbc_block_lba2sector(dev, start_lba);
    ret = ioctl(dev->zbd_fd, BLKFINISHZONE, &sector);
    if ( ret != 0 ) {
	ret = -errno;
	zbc_error("%s: ioctl BLKFINISHZONE failed %d (%s)\n",
		  dev->zbd_filename,
		  errno,
		  strerror(errno));
	return ret;
    }

    return 0;

}

/**
 * Reset zone(s) write pointer: use BLKDISCARD ioctl.
 */
static int
zbc_block_reset_wp(struct zbc_device *dev,
		   uint64_t start_lba)
{
    struct zbc_zone zone;
    unsigned int nr_zones, n;
    uint64_t sector;
    int ret;

    if ( start_lba == (uint64_t)-1 ) {
        /* Reset all zones */
	nr_zones = UINT_MAX;
	start_lba = 0;
    } else {
        /* Reset only the zone at start_lba */
	nr_zones = 1;
    }

    while( (nr_zones > 0) && (start_lba < dev->zbd_info.zbd_logical_blocks) ) {

	/* Get the zone info */
	n = 1;
	ret = zbc_block_report_zones(dev, start_lba, ZBC_RO_ALL, NULL, &zone, &n);
	if ( ! n ) {
	    ret = -EINVAL;
	}
	if ( ret != 0 ) {
	    return ret;
	}

	/* Reset the zone */
	if ( zbc_zone_sequential(&zone)
	     && (! zbc_zone_empty(&zone)) ) {
	    sector = zbc_block_lba2sector(dev, start_lba);
	    ret = ioctl(dev->zbd_fd, BLKRESETZONE, &sector);
	    if ( ret != 0 ) {
		ret = -errno;
		zbc_error("%s: ioctl BLKRESETZONE failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		return ret;
	    }
	}

	start_lba += zone.zbz_length;
	nr_zones--;

    }

    return ret;

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
    if ( ret < 0 ) {
        ret = -errno;
    } else {
        ret = zbc_block_bytes2lba(dev, ret);
    }

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
    if ( ret < 0 ) {
        ret = -errno;
    } else {
        ret = zbc_block_bytes2lba(dev, ret);
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
