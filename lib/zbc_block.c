/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */

/***** Including files *****/

#include "zbc.h"
#include "zbc_sg.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>

#ifdef HAVE_LINUX_BLKZONED_H
#include <linux/blkzoned.h>
#endif

/**
 * Block device descriptor data.
 */
struct zbc_block_device {

	struct zbc_device	dev;

	int			is_part;

	char			*holder_name;

	char			*part_name;
	unsigned long long	part_offset;

};

/**
 * zbc_dev_to_block - Convert device address to block device address.
 */
static inline struct zbc_block_device *
zbc_dev_to_block(struct zbc_device *dev)
{
	return container_of(dev, struct zbc_block_device, dev);
}

/**
 * Get the start sector offset of a partition device.
 */
static int
zbc_block_get_partition_start(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char str[128];
	FILE *file;

	/* Open the start offset file of the partition */
	snprintf(str, sizeof(str),
		 "/sys/block/%s/%s/start",
		 zbd->holder_name,
		 zbd->part_name);
	file = fopen(str, "r");
	if (!file)
		return -EIO;

	fscanf(file, "%llu", &zbd->part_offset);
	fclose(file);

	zbc_debug("%s: Partition of %s, start sector offset %llu\n",
		  dev->zbd_filename,
		  zbd->holder_name,
		  zbd->part_offset);

	return 0;
}

/**
 * Open the holder device for a partition device.
 * the holder file descriptor will be used for
 * handling SG_IO calls for open, close and finish zones.
 */
static int
zbc_block_open_holder(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char str[128];
	int ret;

	/* Open the start offset file of the partition */
	snprintf(str, sizeof(str),
		 "%s/%s",
		 dirname(dev->zbd_filename),
		 zbd->holder_name);

	dev->zbd_sg_fd = open(str, O_RDWR | O_LARGEFILE);
	if (dev->zbd_sg_fd < 0) {
		ret = -errno;
		zbc_error("%s: open holder device failed %d (%s)\n",
			  zbd->part_name,
			  errno,
			  strerror(errno));
		return ret;
	}

	return 0;
}

/**
 * Test if the block device is a partition.
 * If yes, find out the holder device name and get the partition start offset.
 */
static int
zbc_block_handle_partition(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	unsigned long long size;
	unsigned int major, minor = 0;
	unsigned int dev_minor;
	char *dev_name = basename(dev->zbd_filename);
	char str[128];
	char part_name[128];
	FILE *file;
	int ret = 0;

	zbd->is_part = 0;
	zbd->part_name = dev_name;

	/* Check that this is a zoned block device */
	file = fopen("/proc/partitions", "r");
	if (!file) {
		ret = -EIO;
		goto not_part;
	}

	fgets(str, sizeof(str), file);
	fgets(str, sizeof(str), file);
	while (1) {

		ret = fscanf(file,
			     " %u %u %llu %s",
			     &major, &minor, &size, part_name);
		if (ret != 4)
			continue;

		if (strcmp(dev_name, part_name) == 0) {
			zbd->is_part = 1;
			break;
		}

	}

	if (!zbd->is_part) {
not_part:
		zbd->holder_name = strdup(dev_name);
		zbd->part_offset = 0;
		dev->zbd_sg_fd = dev->zbd_fd;
		goto out;
	}

	/* Get the partition holder name */
	dev_minor = minor & ~15U;
	rewind(file);
	fgets(str, sizeof(str), file);
	fgets(str, sizeof(str), file);
	while (1) {

		ret = fscanf(file,
			     " %u %u %llu %s",
			     &major, &minor, &size, part_name);
		if (ret != 4)
			continue;

		if (minor == dev_minor) {
			zbd->holder_name = strdup(part_name);
			break;
		}

	}

	if (!zbd->holder_name) {
		zbd->is_part = 0;
		goto not_part;
	}

	ret = zbc_block_get_partition_start(dev);
	if (ret == 0)
		ret = zbc_block_open_holder(dev);

out:
	fclose(file);

	return ret;
}

/**
 * Test if the block device is zoned.
 */
static int
zbc_block_device_is_zoned(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char str[128];
	FILE *file;

	/* Check that this is a zoned block device */
	snprintf(str, sizeof(str),
		 "/sys/block/%s/queue/zoned",
		 zbd->holder_name);
	file = fopen(str, "r");
	if (!file)
		return 0;

	memset(str, 0, sizeof(str));
	fscanf(file, "%s", str);
	fclose(file);

	if (strcmp(str, "host-aware") == 0) {
		dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
		return 1;
	}

	if (strcmp(str, "host-managed") == 0) {
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
zbc_block_get_str(FILE *file, char *str)
{
	int len = 0;

	if (fgets(str, 128, file)) {
		len = strlen(str) - 1;
		while (len > 0) {
			if (str[len] == ' ' ||
			    str[len] == '\t' ||
			    str[len] == '\r' ||
			    str[len] == '\n') {
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
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char str[128];
	FILE *file;
	int n = 0, len;

	snprintf(str, sizeof(str),
		 "/sys/block/%s/device/vendor",
		 zbd->holder_name);
	file = fopen(str, "r");
	if (file) {
		len = zbc_block_get_str(file, str);
		if (len)
			n = snprintf(dev->zbd_info.zbd_vendor_id,
				     ZBC_DEVICE_INFO_LENGTH,
				     "%s ", str);
		fclose(file);
	}

	snprintf(str, sizeof(str),
		 "/sys/block/%s/device/model",
		 zbd->holder_name);
	file = fopen(str, "r");
	if (file) {
		len = zbc_block_get_str(file, str);
		if (len)
			n += snprintf(&dev->zbd_info.zbd_vendor_id[n],
				      ZBC_DEVICE_INFO_LENGTH - n,
				      "%s ", str);
		fclose(file);
	}

	snprintf(str, sizeof(str),
		 "/sys/block/%s/device/rev",
		 zbd->holder_name);
	file = fopen(str, "r");
	if (file) {
		len = zbc_block_get_str(file, str);
		if (len)
			n += snprintf(&dev->zbd_info.zbd_vendor_id[n],
				      ZBC_DEVICE_INFO_LENGTH - n,
				      "%s", str);
		fclose(file);
	}

	return n > 0;
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
	if (fstat(dev->zbd_fd, &st) < 0) {
		ret = -errno;
		zbc_error("%s: stat failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		return ret;
	}

	if (!S_ISBLK(st.st_mode))
		/* Not a block device: ignore */
		return -ENXIO;

	/* Check if we are dealing with a partition */
	ret = zbc_block_handle_partition(dev);
	if (ret)
		return ret;

	/* Is this a zoned device ? And do we have kernel support ? */
	if (!zbc_block_device_is_zoned(dev))
		/* Not a zoned block device: ignore */
		return -ENXIO;

	/* Get logical block size */
	ret = ioctl(dev->zbd_fd, BLKSSZGET, &size32);
	if (ret != 0) {
		ret = -errno;
		zbc_error("%s: ioctl BLKSSZGET failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		return ret;
	}
	dev->zbd_info.zbd_lblock_size = size32;

	/* Get physical block size */
	ret = ioctl(dev->zbd_fd, BLKPBSZGET, &size32);
	if (ret != 0) {
		ret = -errno;
		zbc_error("%s: ioctl BLKPBSZGET failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		return ret;
	}
	dev->zbd_info.zbd_pblock_size = size32;

	/* Get capacity (Bytes) */
	ret = ioctl(dev->zbd_fd, BLKGETSIZE64, &size64);
	if (ret != 0) {
		ret = -errno;
		zbc_error("%s: ioctl BLKGETSIZE64 failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		return ret;
	}

	if (dev->zbd_info.zbd_lblock_size <= 0) {
		zbc_error("%s: invalid logical sector size %d\n",
			  dev->zbd_filename,
			  size32);
		return -EINVAL;
	}
	dev->zbd_info.zbd_lblocks = size64 / dev->zbd_info.zbd_lblock_size;

	if (dev->zbd_info.zbd_pblock_size <= 0) {
		zbc_error("%s: invalid physical sector size %d\n",
			  dev->zbd_filename,
			  size32);
		return -EINVAL;
	}
	dev->zbd_info.zbd_pblocks = size64 / dev->zbd_info.zbd_pblock_size;

	/* Check */
	if (!dev->zbd_info.zbd_lblocks) {
		zbc_error("%s: invalid capacity (logical blocks)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	if (!dev->zbd_info.zbd_pblocks) {
		zbc_error("%s: invalid capacity (physical blocks)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Finish setting */
	dev->zbd_info.zbd_type = ZBC_DT_BLOCK;
	if (!zbc_block_get_vendor_id(dev))
		strncpy(dev->zbd_info.zbd_vendor_id,
			"Unknown", ZBC_DEVICE_INFO_LENGTH - 1);

	/*
	 * Use SG_IO to get zone characteristics
	 * (maximum number of open zones, etc).
	 */
	if (zbc_scsi_get_zbd_characteristics(dev))
		return -ENXIO;

	/* Get maximum command size */
	zbc_sg_get_max_cmd_blocks(dev);

	dev->zbd_info.zbd_sectors =
		(dev->zbd_info.zbd_lblocks *
		 dev->zbd_info.zbd_lblock_size) >> 9;

	return 0;
}

/**
 * Open a block device.
 */
static int zbc_block_open(const char *filename, int flags,
			  struct zbc_device **pdev)
{
	struct zbc_block_device *zbd;
	struct zbc_device *dev;
	int fd, ret;

	zbc_debug("%s: ########## Trying BLOCK driver ##########\n",
		  filename);

#ifndef HAVE_LINUX_BLKZONED_H
	zbc_debug("libzbc compiled without block driver support\n");
	return -ENXIO;
#endif

	/* Open block device: always add write mode for discard (reset zone) */
	fd = open(filename, flags | O_LARGEFILE);
	if (fd < 0) {
		ret = -errno;
		zbc_error("%s: open failed %d (%s)\n",
			  filename,
			  errno,
			  strerror(errno));
		goto out;
	}

	/* Allocate a handle */
	ret = -ENOMEM;
	zbd = calloc(1, sizeof(struct zbc_block_device));
	if (!zbd)
		goto out;

	dev = &zbd->dev;
	dev->zbd_fd = fd;
	dev->zbd_filename = strdup(filename);
	if (!dev->zbd_filename)
		goto out_free_dev;

	/* Get device information */
	ret = zbc_block_get_info(dev);
	if (ret != 0)
		goto out_free_filename;

	*pdev = dev;

	zbc_debug("%s: ########## BLOCK driver succeeded ##########\n",
		  filename);

	return 0;

out_free_filename:
	free(dev->zbd_filename);

out_free_dev:
	free(zbd);

out:
	if (fd >= 0)
		close(fd);

	zbc_debug("%s: ########## BLOCK driver failed %d ##########\n",
		  filename, ret);

	return ret;
}

/**
 * Close a device.
 */
static int zbc_block_close(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	int ret = 0;

	/* Close device */
	if (close(dev->zbd_fd) < 0)
		ret = -errno;

	if (ret == 0) {
		if (zbd->is_part)
			close(dev->zbd_sg_fd);
		free(zbd->holder_name);
		free(dev->zbd_filename);
		free(zbd);
	}

	return ret;
}

#ifdef HAVE_LINUX_BLKZONED_H

/**
 * Test if a zone should be reported depending
 * on the specified reporting options.
 */
static bool
zbc_block_must_report(struct zbc_zone *zone, uint64_t start_sector,
		      enum zbc_reporting_options ro)
{
	enum zbc_reporting_options options = zbc_ro_mask(ro);

	if (zone->zbz_start + zone->zbz_length < start_sector)
		return false;

	switch (options) {
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
	case ZBC_RO_RWP_RECOMMENDED:
		return zbc_zone_rwp_recommended(zone);
	case ZBC_RO_NON_SEQ:
		return zbc_zone_non_seq(zone);
	case ZBC_RO_NOT_WP:
		return zbc_zone_not_wp(zone);
	default:
		return false;
    }
}

#define ZBC_BLOCK_ZONE_REPORT_NR_ZONES	8192

/**
 * Get the block device zone information.
 */
static int zbc_block_report_zones(struct zbc_device *dev, uint64_t start_sector,
				  enum zbc_reporting_options ro,
				  struct zbc_zone *zones,
				  unsigned int *nr_zones)
{
	size_t rep_size;
	uint64_t sector = start_sector;
	struct zbc_zone zone;
	struct blk_zone_report *rep;
	struct blk_zone *blkz;
	unsigned int i, n = 0;
	int ret;

	rep_size = sizeof(struct blk_zone_report) +
		sizeof(struct blk_zone) * ZBC_BLOCK_ZONE_REPORT_NR_ZONES;
	rep = malloc(rep_size);
	if (!rep) {
		zbc_error("%s: No memory for report zones\n",
			  dev->zbd_filename);
		return -ENOMEM;
	}
	blkz = (struct blk_zone *)(rep + 1);

	while (((! *nr_zones) || (n < *nr_zones)) &&
	       (sector < dev->zbd_info.zbd_sectors)) {

		/* Get zone info */
		memset(rep, 0, rep_size);
		rep->sector = sector;
		rep->nr_zones = ZBC_BLOCK_ZONE_REPORT_NR_ZONES;

		ret = ioctl(dev->zbd_fd, BLKREPORTZONE, rep);
		if (ret != 0) {
			ret = -errno;
			zbc_error("%s: ioctl BLKREPORTZONE at %llu failed %d (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long)sector,
				  errno,
				  strerror(errno));
			goto out;
		}

		for (i = 0; i < rep->nr_zones; i++) {

			if ((*nr_zones && (n >= *nr_zones)) ||
			    (sector >= dev->zbd_info.zbd_sectors))
				break;

			memset(&zone, 0, sizeof(struct zbc_zone));
			zone.zbz_type = blkz[i].type;
			zone.zbz_condition = blkz[i].cond;
			zone.zbz_length = blkz[i].len;
			zone.zbz_start = blkz[i].start;
			zone.zbz_write_pointer = blkz[i].wp;
			if (blkz[i].reset)
				zone.zbz_attributes |= ZBC_ZA_RWP_RECOMMENDED;
			if (blkz[i].non_seq)
				zone.zbz_attributes |= ZBC_ZA_NON_SEQ;

			if (zbc_block_must_report(&zone, start_sector, ro)) {
				if (zones)
					memcpy(&zones[n], &zone,
					       sizeof(struct zbc_zone));
				n++;
			}

			sector = zone.zbz_start + zone.zbz_length;

		}

	}

	/* Return number of zones */
	*nr_zones = n;

out:
	free(rep);

	return ret;
}

/**
 * Reset a single zone write pointer.
 */
static int zbc_block_reset_one(struct zbc_device *dev, uint64_t sector)
{
	struct blk_zone_range range;
	unsigned int nr_zones = 1;
	struct zbc_zone zone;
	int ret;

	/* Get zone info */
	ret = zbc_block_report_zones(dev, sector, ZBC_RO_ALL,
				     &zone, &nr_zones);
	if (ret)
		return ret;

	if (!nr_zones) {
		zbc_error("%s: Invalid zone sector %llu\n",
			  dev->zbd_filename,
			  (unsigned long long)sector);
		return -EINVAL;
	}

	if (zbc_zone_conventional(&zone)
	    || zbc_zone_empty(&zone))
		/* Nothing to do */
		return 0;

	/* Reset zone */
	range.sector = zbc_zone_start(&zone);
	range.nr_sectors = zbc_zone_length(&zone);
	ret = ioctl(dev->zbd_fd, BLKRESETZONE, &range);
	if (ret != 0) {
		ret = -errno;
		zbc_error("%s: ioctl BLKRESETZONE failed %d (%s)\n",
			  dev->zbd_filename,
			  errno, strerror(errno));
		return ret;
	}

	return 0;
}

/**
 * Reset all zones write pointer.
 */
static int zbc_block_reset_all(struct zbc_device *dev)
{
	struct zbc_zone *zones;
	unsigned int i, nr_zones;
	struct blk_zone_range range;
	uint64_t sector = 0;
	int ret;

	zones = calloc(ZBC_BLOCK_ZONE_REPORT_NR_ZONES,
		       sizeof(struct zbc_zone));
	if (!zones) {
		zbc_error("%s: No memory for report zones\n",
			  dev->zbd_filename);
		return -ENOMEM;
	}

	while (1) {

		/* Get zone info */
		nr_zones = ZBC_BLOCK_ZONE_REPORT_NR_ZONES;
		ret = zbc_block_report_zones(dev, sector, ZBC_RO_ALL,
					     zones, &nr_zones);
		if (ret || !nr_zones)
			break;

		for (i = 0; i < nr_zones; i++) {

			sector = zones[i].zbz_start + zones[i].zbz_length;

			if (zbc_zone_conventional(&zones[i])
			    || zbc_zone_empty(&zones[i]))
				continue;

			/* Reset zone */
			range.sector = zones[i].zbz_start;
			range.nr_sectors = zones[i].zbz_length;
			ret = ioctl(dev->zbd_fd, BLKRESETZONE, &range);
			if (ret != 0) {
				ret = -errno;
				zbc_error("%s: ioctl BLKRESETZONE failed %d (%s)\n",
					  dev->zbd_filename,
					  errno, strerror(errno));
				break;
			}

		}

	}

	free(zones);

	return ret;
}

/**
 * Execute an operation on a zone
 */
static int
zbc_block_zone_op(struct zbc_device *dev, uint64_t sector,
		  enum zbc_zone_op op, unsigned int flags)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	uint64_t sect = sector;

	switch (op) {

	case ZBC_OP_RESET_ZONE:

		if (flags & ZBC_OP_ALL_ZONES)
			/* All zones */
			return zbc_block_reset_all(dev);

		/* One zone */
		return zbc_block_reset_one(dev, sector);

	case ZBC_OP_OPEN_ZONE:
	case ZBC_OP_CLOSE_ZONE:
	case ZBC_OP_FINISH_ZONE:

		if (zbd->is_part)
			sect += zbd->part_offset;
		/* Use SG_IO */
		return zbc_scsi_zone_op(dev, sect, op, flags);

	default:
		zbc_error("Invalid operation code 0x%x\n", op);
		return -EINVAL;
	}
}

/**
 * Read from the block device.
 */
static ssize_t zbc_block_pread(struct zbc_device *dev, void *buf,
			       size_t count, uint64_t offset)
{
	ssize_t ret;

	/* Read */
	ret = pread(dev->zbd_fd, buf, count << 9, offset << 9);
	if (ret < 0)
		return -errno;

	return ret >> 9;
}

/**
 * Write to the block device.
 */
static ssize_t zbc_block_pwrite(struct zbc_device *dev,
				const void *buf,
				size_t count,
				uint64_t offset)
{
	ssize_t ret;

	/* Read */
	ret = pwrite(dev->zbd_fd, buf, count << 9, offset << 9);
	if (ret < 0)
		return -errno;

	return ret >> 9;
}

/**
 * Flush the device.
 */
static int zbc_block_flush(struct zbc_device *dev)
{
	return fsync(dev->zbd_fd);
}

#else /* HAVE_LINUX_BLKZONED_H */

static int zbc_block_report_zones(struct zbc_device *dev, uint64_t sector,
				  enum zbc_reporting_options ro,
				  struct zbc_zone *zones,
				  unsigned int *nr_zones)
{
    return -EOPNOTSUPP;
}

static int zbc_block_zone_op(struct zbc_device *dev, uint64_t sector,
			     enum zbc_zone_op op, unsigned int flags)
{
    return -EOPNOTSUPP;
}

static ssize_t zbc_block_pread(struct zbc_device *dev, void *buf,
			       size_t count, uint64_t offset)
{
    return -EOPNOTSUPP;
}

static ssize_t zbc_block_pwrite(struct zbc_device *dev, const void *buf,
			       size_t count, uint64_t offset)
{
    return -EOPNOTSUPP;
}

static int zbc_block_flush(struct zbc_device *dev)
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
    .zbd_zone_op      = zbc_block_zone_op,
};
