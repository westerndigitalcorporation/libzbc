// SPDX-License-Identifier: BSD-2-Clause
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include "zbc.h"
#include "zbc_utils.h"
#include "zbc_sg.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>

#ifdef HAVE_LINUX_BLKZONED_H

#include <linux/blkzoned.h>

/**
 * Block device descriptor data.
 */
struct zbc_block_device {

	struct zbc_device	dev;

	int			is_part;
	int			is_scsi_dev;

	char			*holder_name;

	char			*part_name;
	unsigned long long	part_offset;

	unsigned long long	zone_sectors;
};

/*
 * Default values for zoned block device characteristics
 * used if we are dealing with a device mapper block device.
 */
#define ZBC_BLOCK_MAX_OPEN_ZONES		128

/**
 * zbc_dev_to_block - Convert device address to block device address.
 */
static inline struct zbc_block_device *zbc_dev_to_block(struct zbc_device *dev)
{
	return container_of(dev, struct zbc_block_device, dev);
}

static int dir_has(const char *dir, const char *entry)
{
	struct dirent *e;
	int res = 0;
	DIR *d;

	d = opendir(dir);
	if (!d) {
		/* If dir does not exist, entry does not exist either */
		if (errno == ENOENT)
			return 0;
		return -errno;
	}
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, entry) == 0) {
			res = 1;
			break;
		}
	}
	closedir(d);
	return res;
}

static int zbc_block_is_scsi_dev(const char *zbd_filename)
{
	struct dirent *e;
	char *path;
	int res = 0;
	DIR *d;

	d = opendir("/sys/class/scsi_device");
	if (!d)
		goto out;
	while ((e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.')
			continue;
		if (asprintf(&path, "/sys/class/scsi_device/%s/device/block",
			     e->d_name) < 0) {
			res = -ENOMEM;
			break;
		}
		res = dir_has(path, zbd_filename + 5);
		free(path);
		if (res > 0)
			break;
	}
	closedir(d);
out:
	return res;
}

/**
 * Get the start sector offset of a partition device.
 */
static int zbc_block_get_partition_start(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char sysfs_path[128];
	int ret;

	/* Open the start offset file of the partition */
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/block/%s/%s/start",
		 zbd->holder_name,
		 zbd->part_name);
	ret = zbc_get_sysfs_val_ull(sysfs_path, &zbd->part_offset);
	if (ret) {
		zbc_error("%s: can't read partition offset from %s\n",
			  zbd->part_name, sysfs_path);
		return ret;
	}

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
static int zbc_block_open_holder(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char *zbd_filename = strdup(dev->zbd_filename);
	char str[128];
	int ret = 0;

	/* Open the holder device of the partition */
	if (!zbd_filename)
		return -ENOMEM;

	snprintf(str, sizeof(str),
		 "%s/%s",
		 dirname(zbd_filename),
		 zbd->holder_name);

	dev->zbd_sg_fd = open(str, O_RDWR | O_LARGEFILE);
	if (dev->zbd_sg_fd < 0) {
		ret = -errno;
		zbc_error("%s: open holder device %s failed %d (%s)\n",
			  dev->zbd_filename, str,
			  errno, strerror(errno));
	}

	free(zbd_filename);

	return ret;
}

/**
 * Test if the block device is a partition.
 * If yes, find out the holder device name and get the partition start offset.
 */
static int zbc_block_handle_partition(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	struct stat statbuf;
	char *dev_name = basename(dev->zbd_filename);
	char *path;
	DIR *d;
	struct dirent *e;
	int ret = 0;

	zbd->part_name = dev_name;

	if (asprintf(&path, "/sys/class/block/%s/partition", dev_name) < 0)
		return -ENOMEM;
	zbd->is_part = stat(path, &statbuf) == 0;
	free(path);

	if (!zbd->is_part) {
not_part:
		zbd->holder_name = strdup(dev_name);
		zbd->part_offset = 0;
		dev->zbd_sg_fd = dev->zbd_fd;
		goto out;
	}

	d = opendir("/sys/block");
	while (d && !zbd->holder_name && (e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.')
			continue;
		if (asprintf(&path, "/sys/block/%s/%s", e->d_name, dev_name) < 0)
			continue;
		if (stat(path, &statbuf) == 0)
			zbd->holder_name = strdup(e->d_name);
		free(path);
	}
	closedir(d);

	if (!zbd->holder_name) {
		zbd->is_part = 0;
		goto not_part;
	}

	ret = zbc_block_get_partition_start(dev);
	if (ret == 0)
		ret = zbc_block_open_holder(dev);

out:
	return ret;
}

/**
 * Test if the block device is zoned.
 */
static int zbc_block_device_classify(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char model[32];
	int ret;

	/* Check that this is a zoned block device */
	ret = zbc_get_sysfs_queue_str(zbd->holder_name, "zoned",
				      model, sizeof(model));
	if (ret)
		/*
		 * Cannot determine type: go on with SCSI,
		 * ATA or fake backends.
		 */
		return -ENXIO;

	if (strcmp(model, "host-aware") == 0) {
		dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
	} else if (strcmp(model, "host-managed") == 0) {
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
	} else if (strcmp(model, "none") == 0) {
		dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
		return -ENXIO;
	} else {
		zbc_debug("%s: Unknown device model \"%s\"\n",
			  dev->zbd_filename, model);
		dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
		return -ENXIO;
	}

	return 0;
}

/**
 * Get vendor ID.
 */
static int zbc_block_get_vendor_id(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	char str[128];
	int ret, n = 0;

	ret = zbc_get_sysfs_device_str(zbd->holder_name, "vendor",
				       str, sizeof(str));
	if (!ret)
		n = snprintf(dev->zbd_info.zbd_vendor_id,
			     ZBC_DEVICE_INFO_LENGTH,
			     "%s ", str);

	ret = zbc_get_sysfs_device_str(zbd->holder_name, "model",
				       str, sizeof(str));
	if (!ret)
		n += snprintf(&dev->zbd_info.zbd_vendor_id[n],
			      ZBC_DEVICE_INFO_LENGTH - n,
			      "%s ", str);

	ret = zbc_get_sysfs_device_str(zbd->holder_name, "rev",
				       str, sizeof(str));
	if (!ret)
		n += snprintf(&dev->zbd_info.zbd_vendor_id[n],
			      ZBC_DEVICE_INFO_LENGTH - n,
			      "%s", str);

	return n > 0;
}

/**
 * Get the zone size reported by the kernel.
 */
static int zbc_block_get_zone_sectors(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	int ret;

	/* Open the chunk_sectors file */
	ret = zbc_get_sysfs_queue_val_ull(zbd->holder_name, "chunk_sectors",
					  &zbd->zone_sectors);
	if (ret) {
		zbc_error("%s: get zone sectors from sysfs failed\n",
			  zbd->part_name);
		return ret;
	}

	zbc_debug("%s: Zones of %llu sectors\n",
		  zbd->part_name, zbd->zone_sectors);

	return 0;
}

/**
 * Get the maximum number of open zones of a device.
 */
static unsigned int zbc_block_get_max_open_zones(struct zbc_device *dev)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	unsigned long long val;
	int ret;

	/* Open the max_open_zones file */
	ret = zbc_get_sysfs_queue_val_ull(zbd->holder_name,
					  "max_open_zones", &val);
	if (ret)
		return ZBC_BLOCK_MAX_OPEN_ZONES;

	return val;
}

/**
 * Test if the device can be handled and get the block device info.
 */
static int zbc_block_get_info(struct zbc_device *dev, struct stat *st)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	unsigned long long size64;
	int size32;
	int ret;

	/*
	 * We do not need zbc_report_zones() to allocate a buffer for
	 * report zones. zbc_block_report_zones() does its own allocation
	 * as needed.
	 */
	dev->zbd_report_bufsz_min = 0;
	dev->zbd_report_bufsz_mask = 0;

	/* Check if we are dealing with a partition */
	ret = zbc_block_handle_partition(dev);
	if (ret)
		return ret;

	/* Is this a zoned device */
	ret = zbc_block_device_classify(dev);
	if (ret != 0)
		return ret;

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

	if (zbc_block_get_zone_sectors(dev))
		return -EINVAL;

	/* Finish setting */
	dev->zbd_info.zbd_type = ZBC_DT_BLOCK;
	if (!zbc_block_get_vendor_id(dev))
		strncpy(dev->zbd_info.zbd_vendor_id,
			"Unknown", ZBC_DEVICE_INFO_LENGTH - 1);

	ret = zbc_block_is_scsi_dev(dev->zbd_filename);
	if (ret < 0)
		return ret;
	zbd->is_scsi_dev = ret;

	if (!zbd->is_scsi_dev) {
		/* Use defaults for non-SCSI devices */
		dev->zbd_info.zbd_flags |= ZBC_UNRESTRICTED_READ;
		if (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED) {
			dev->zbd_info.zbd_max_nr_open_seq_req =
				zbc_block_get_max_open_zones(dev);
			dev->zbd_info.zbd_opt_nr_open_seq_pref = 0;
			dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref = 0;
		} else {
			dev->zbd_info.zbd_max_nr_open_seq_req = 0;
			dev->zbd_info.zbd_opt_nr_open_seq_pref =
				ZBC_NOT_REPORTED;
			dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref =
				ZBC_NOT_REPORTED;
		}
	} else if (zbc_scsi_get_zbd_characteristics(dev)) {
		return -ENXIO;
	}

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
	struct stat st;
	int fd = -1, ret;

	zbc_debug("%s: ########## Trying BLOCK driver ##########\n",
		  filename);

	/* Check device */
	if (stat(filename, &st) != 0) {
		ret = -errno;
		zbc_error("%s: Stat device file failed %d (%s)\n",
			  filename,
			  errno, strerror(errno));
		return ret;
	}

	if (!S_ISBLK(st.st_mode)) {
		ret = -ENXIO;
		goto out;
	}

	/* Open block device */
	fd = open(filename, (flags & ZBC_O_DMODE_MASK) | O_LARGEFILE);
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
	ret = zbc_block_get_info(dev, &st);
	if (ret != 0)
		goto out_free_filename;

	*pdev = dev;

	zbc_debug("%s: ########## BLOCK driver succeeded ##########\n\n",
		  filename);

	return 0;

out_free_filename:
	if (zbd->holder_name)
		free(zbd->holder_name);

	free(dev->zbd_filename);

out_free_dev:
	free(zbd);

out:
	if (fd >= 0)
		close(fd);

	zbc_debug("%s: ########## BLOCK driver failed %d ##########\n\n",
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

/**
 * Test if a zone should be reported depending
 * on the specified reporting options.
 */
static bool zbc_block_must_report(struct zbc_zone *zone, uint64_t start_sector,
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
				  struct zbc_zone *zones, unsigned int *nr_zones)
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

		if (!rep->nr_zones)
			break;

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
	ret = zbc_block_report_zones(dev, sector, ZBC_RO_ALL, &zone, &nr_zones);
	if (ret)
		return ret;

	if (!nr_zones) {
		zbc_error("%s: Invalid zone sector %llu\n",
			  dev->zbd_filename,
			  (unsigned long long)sector);
		return -EINVAL;
	}

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
 * Reset all zones write pointer using kernel path.
 */
static int zbc_block_reset_all_fast(struct zbc_device *dev)
{
	struct blk_zone_range range;
	int ret;

	range.sector = 0;
	range.nr_sectors = dev->zbd_info.zbd_sectors;
	ret = ioctl(dev->zbd_fd, BLKRESETZONE, &range);
	if (ret)
		return -errno;

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
	uint64_t sector = 0, seq_sector = 0;
	uint64_t nr_seq_sectors;
	int ret;

	/*
	 * Kernel 5.4 added support for reset all execution as a single command.
	 * Try it here and if this fails, falls back to the slow path
	 * inspecting all zones one by one.
	 */
	ret = zbc_block_reset_all_fast(dev);
	if (!ret)
		return 0;

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

		i = 0;

		while (i < nr_zones) {

			nr_seq_sectors = 0;

			while (i < nr_zones) {

				sector = zones[i].zbz_start +
					zones[i].zbz_length;

				if (!zbc_zone_conventional(&zones[i]) &&
				    !zbc_zone_empty(&zones[i])) {
					if (!nr_seq_sectors)
						seq_sector = zones[i].zbz_start;
					nr_seq_sectors += zones[i].zbz_length;
				}

				if ((zbc_zone_conventional(&zones[i]) ||
				     zbc_zone_empty(&zones[i])) &&
				    nr_seq_sectors)
						break;

				i++;

			}

			if (!nr_seq_sectors)
				continue;

			/* Reset zones */
			range.sector = seq_sector;
			range.nr_sectors = nr_seq_sectors;
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
 * Execute a zone management operation using the scsi backend.
 */
static int zbc_block_zone_op_scsi(struct zbc_device *dev, uint64_t sector,
				  enum zbc_zone_op op, unsigned int flags)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);

	if (!zbd->is_scsi_dev) {
		zbc_error("%s: Not a SCSI device (operation not supported)\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	if (zbd->is_part)
		sector += zbd->part_offset;

	/* Use SG_IO */
	return zbc_scsi_zone_op(dev, sector, op, flags);
}

/**
 * Execute a zone management operation using the kernel ioctl commands
 * introduced in kernel 5.5.
 */
static int zbc_block_zone_op_ioctl(struct zbc_device *dev, uint64_t sector,
				   enum zbc_zone_op op, unsigned int flags)
{
	struct zbc_block_device *zbd = zbc_dev_to_block(dev);
	struct blk_zone_range range;
	int ret, cmd;

	if (flags & ZBC_OP_ALL_ZONES)
		return -EOPNOTSUPP;

	switch (op) {
#ifdef BLKOPENZONE
	case ZBC_OP_OPEN_ZONE:
		cmd = BLKOPENZONE;
		break;
#endif
#ifdef BLKCLOSEZONE
	case ZBC_OP_CLOSE_ZONE:
		cmd = BLKCLOSEZONE;
		break;
#endif
#ifdef BLKFINISHZONE
	case ZBC_OP_FINISH_ZONE:
		cmd = BLKFINISHZONE;
		break;
#endif
	default:
		return -EOPNOTSUPP;
	}

	range.sector = sector;
	range.nr_sectors = zbd->zone_sectors;
	ret = ioctl(dev->zbd_fd, cmd, &range);
	if (ret) {
		ret = -errno;
		zbc_error("%s: ioctl 0x%x failed %d (%s)\n",
			  dev->zbd_filename, cmd, errno, strerror(errno));
		return ret;
	}

	return 0;
}

/**
 * Execute an operation on a zone
 */
static int zbc_block_zone_op(struct zbc_device *dev, uint64_t sector,
			     enum zbc_zone_op op, unsigned int flags)
{
	int ret;

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

		/*
		 * Try kernel ioctl first. If that fails, fallback to scsi
		 * operation.
		 */
		ret = zbc_block_zone_op_ioctl(dev, sector, op, flags);
		if (ret != -EOPNOTSUPP)
			return ret;

		return zbc_block_zone_op_scsi(dev, sector, op, flags);

	default:
		zbc_error("%s: Invalid operation code 0x%x\n",
			  dev->zbd_filename, op);
		return -EINVAL;
	}
}

/**
 * Read from the block device.
 */
static ssize_t zbc_block_preadv(struct zbc_device *dev,
				const struct iovec *iov, int iovcnt,
			        uint64_t offset)
{
	ssize_t ret;

	ret = preadv(dev->zbd_fd, iov, iovcnt, offset << 9);
	if (ret < 0)
		return -errno;

	return ret >> 9;
}

/**
 * Write to the block device.
 */
static ssize_t zbc_block_pwritev(struct zbc_device *dev,
				 const struct iovec *iov, int iovcnt,
			         uint64_t offset)
{
	ssize_t ret;

	ret = pwritev(dev->zbd_fd, iov, iovcnt, offset << 9);
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

static int zbc_block_open(const char *filename, int flags,
			  struct zbc_device **pdev)
{
	zbc_debug("libzbc compiled without zoned block device driver support\n");

	return -ENXIO;
}

static int zbc_block_close(struct zbc_device *dev)
{
	return -EOPNOTSUPP;
}

static int zbc_block_report_zones(struct zbc_device *dev, uint64_t sector,
				  enum zbc_reporting_options ro,
				  struct zbc_zone *zones, unsigned int *nr_zones)
{
	return -EOPNOTSUPP;
}

static int zbc_block_zone_op(struct zbc_device *dev, uint64_t sector,
			     enum zbc_zone_op op, unsigned int flags)
{
	return -EOPNOTSUPP;
}

static ssize_t zbc_block_preadv(struct zbc_device *dev,
				const struct iovec *iov, int iovcnt,
				uint64_t offset)
{
	return -EOPNOTSUPP;
}

static ssize_t zbc_block_pwritev(struct zbc_device *dev,
				 const struct iovec *iov, int iovcnt,
				 uint64_t offset)
{
	return -EOPNOTSUPP;
}

static int zbc_block_flush(struct zbc_device *dev)
{
	return -EOPNOTSUPP;
}

#endif /* HAVE_LINUX_BLKZONED_H */

/**
 * Zoned block device backend driver definition.
 */
struct zbc_drv zbc_block_drv =
{
	.flag			= ZBC_O_DRV_BLOCK,
	.zbd_open		= zbc_block_open,
	.zbd_close		= zbc_block_close,
	.zbd_preadv		= zbc_block_preadv,
	.zbd_pwritev		= zbc_block_pwritev,
	.zbd_flush		= zbc_block_flush,
	.zbd_report_zones	= zbc_block_report_zones,
	.zbd_zone_op		= zbc_block_zone_op,
};
