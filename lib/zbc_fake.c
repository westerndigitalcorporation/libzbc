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
 * Author: Christoph Hellwig (hch@infradead.org)
 *         Damien Le Moal (damien.lemoal@wdc.com)
 */

#include "zbc.h"
#include "zbc_sg.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <linux/fs.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

/**
 * Logical and physical sector size for emulation on top of a regular file.
 * For emulation on top of a raw block device, the device actual logical and
 * physical block sizes are used.
 */
#define ZBC_FAKE_FILE_BLOCK_SIZE	512

/**
 * Maximum number of open zones (implicit + explicit).
 */
#define ZBC_FAKE_MAX_OPEN_NR_ZONES	32

/*
 * Meta-data directory.
 */
#define ZBC_FAKE_META_DIR		"/var/local"

/*
 * Meta-data maximum path size.
 */
#define ZBC_FAKE_META_PATH_SIZE	512

/**
 * Metadata header.
 */
struct zbc_fake_meta {

	/**
	 * Capacity in B.
	 */
	uint64_t	zbd_capacity;

	/**
	 * Total number of zones.
	 */
	uint32_t	zbd_nr_zones;

	/**
	 * Number of conventional zones.
	 */
	uint32_t	zbd_nr_conv_zones;

	/**
	 * Number of sequential zones.
	 */
	uint32_t	zbd_nr_seq_zones;

	/**
	 * Number of explicitly open zones.
	 */
	uint32_t	zbd_nr_exp_open_zones;

	/**
	 * Number of implicitely open zones.
	 */
	uint32_t	zbd_nr_imp_open_zones;

	uint8_t		reserved[40];

};

/**
 * Fake device descriptor data.
 */
struct zbc_fake_device {

	struct zbc_device	dev;

	int			zbd_meta_fd;
	size_t			zbd_meta_size;
	struct zbc_fake_meta	*zbd_meta;

	uint32_t		zbd_nr_zones;
	struct zbc_zone		*zbd_zones;

};

/**
 * zbc_fake_dev_meta_path - Build metadata file path for a device.
 */
static inline void zbc_fake_dev_meta_path(struct zbc_fake_device *fdev,
					  char *buf)
{
	sprintf(buf, "%s/zbc-%s.meta", ZBC_FAKE_META_DIR,
		basename(fdev->dev.zbd_filename));
}

/**
 * zbc_fake_to_file_dev - Convert device address to fake device address.
 */
static inline struct zbc_fake_device *
zbc_fake_to_file_dev(struct zbc_device *dev)
{
	return container_of(dev, struct zbc_fake_device, dev);
}

/**
 * zbc_fake_find_zone - Find a zone using its start LBA.
 */
static struct zbc_zone *zbc_fake_find_zone(struct zbc_fake_device *fdev,
					   uint64_t sector,
					   bool start)
{
	struct zbc_zone *zone;
	unsigned int i;

	if (!fdev->zbd_zones)
		return NULL;

	for (i = 0; i < fdev->zbd_nr_zones; i++) {
		zone = &fdev->zbd_zones[i];
		if (start) {
			if (zone->zbz_start == sector)
				return zone;
		} else {
			if (sector >= zone->zbz_start &&
			    sector < zone->zbz_start + zone->zbz_length)
				return zone;
		}
	}

	return NULL;
}

/**
 * zbc_fake_lock - Lock a device metadata.
 */
static inline void zbc_fake_lock(struct zbc_fake_device *fdev)
{
	if (flock(fdev->dev.zbd_fd, LOCK_EX) < 0)
		zbc_error("%s: lock metadata failed %d (%s)\n",
			  fdev->dev.zbd_filename,
			  errno, strerror(errno));
	zbc_clear_errno();
}

/**
 * zbc_fake_unlock - Unlock a device metadata.
 */
static inline void zbc_fake_unlock(struct zbc_fake_device *fdev)
{
	if (flock(fdev->dev.zbd_fd, LOCK_UN) < 0)
		zbc_error("%s: unlock metadata failed %d (%s)\n",
			  fdev->dev.zbd_filename,
			  errno, strerror(errno));
}

/**
 * zbc_fake_close_metadata - Close metadata file of a fake device.
 */
static void zbc_fake_close_metadata(struct zbc_fake_device *fdev)
{
	if (fdev->zbd_meta_fd < 0)
		return;

	if (fdev->zbd_meta) {
		msync(fdev->zbd_meta, fdev->zbd_meta_size, MS_SYNC);
		munmap(fdev->zbd_meta, fdev->zbd_meta_size);
		fdev->zbd_meta = NULL;
		fdev->zbd_meta_size = 0;
	}

	close(fdev->zbd_meta_fd);
	fdev->zbd_meta_fd = -1;
}

/**
 * zbc_fake_open_metadata - Open metadata file of a fake device.
 */
static int zbc_fake_open_metadata(struct zbc_fake_device *fdev,
				  bool setzones)
{
	struct zbc_fake_meta *meta;
	struct zbc_device_info *dev_info;
	uint64_t capacity;
	char meta_path[ZBC_FAKE_META_PATH_SIZE];
	struct stat st;
	int ret;

	zbc_fake_dev_meta_path(fdev, meta_path);

	zbc_debug("%s: using meta file %s\n",
		  fdev->dev.zbd_filename,
		  meta_path);

	fdev->zbd_meta_fd = open(meta_path, O_RDWR);
	if (fdev->zbd_meta_fd < 0) {
		/*
		 * Metadata does not exist yet, we'll have to wait
		 * for a set_zones call.
		 */
		if (errno == ENOENT)
			return setzones ? 0 : -ENXIO;
		ret = -errno;
		zbc_error("%s: open metadata file %s failed %d (%s)\n",
			  fdev->dev.zbd_filename,
			  meta_path,
			  errno,
			  strerror(errno));
		goto out;
	}

	if (fstat(fdev->zbd_meta_fd, &st) < 0) {
		ret = -errno;
		zbc_error("%s: fstat metadata file %s failed %d (%s)\n",
			  fdev->dev.zbd_filename,
			  meta_path,
			  errno,
			  strerror(errno));
		goto out;
	}

	/* mmap metadata file */
	fdev->zbd_meta_size = st.st_size;
	fdev->zbd_meta = mmap(NULL, fdev->zbd_meta_size,
			      PROT_READ | PROT_WRITE, MAP_SHARED,
			      fdev->zbd_meta_fd, 0);
	if (fdev->zbd_meta == MAP_FAILED) {
		fdev->zbd_meta = NULL;
		zbc_error("%s: mmap metadata file %s failed\n",
			  fdev->dev.zbd_filename,
			  meta_path);
		ret = -ENOMEM;
		goto out;
	}

	meta = fdev->zbd_meta;
	dev_info = &fdev->dev.zbd_info;

	/* Check */
	capacity = dev_info->zbd_lblock_size * dev_info->zbd_lblocks;
	if (meta->zbd_capacity > capacity || !meta->zbd_nr_zones) {
		/*
		 * Do not report an error here to allow
		 * the execution of zbc_set_zones.
		 */
		zbc_debug("%s: invalid metadata file %s\n",
			  fdev->dev.zbd_filename,
			  meta_path);
		zbc_fake_close_metadata(fdev);
		ret = setzones ? 0 : -ENXIO;
		goto out;
	}

	zbc_debug("%s: %llu sectors of %zuB, %u zones\n",
		  fdev->dev.zbd_filename,
		  (unsigned long long)dev_info->zbd_lblocks,
		  (size_t)dev_info->zbd_lblock_size,
		  meta->zbd_nr_zones);

	fdev->zbd_nr_zones = meta->zbd_nr_zones;
	fdev->zbd_zones = (struct zbc_zone *)(meta + 1);
	if (dev_info->zbd_max_nr_open_seq_req > meta->zbd_nr_seq_zones)
		dev_info->zbd_max_nr_open_seq_req = meta->zbd_nr_seq_zones - 1;
	ret = 0;

out:
	if (ret != 0)
		zbc_fake_close_metadata(fdev);

	return ret;
}

/**
 * zbc_fake_set_info - Set a device info.
 */
static int zbc_fake_set_info(struct zbc_device *dev)
{
	struct zbc_device_info *dev_info = &dev->zbd_info;
	unsigned long long size64;
	struct stat st;
	int size32;
	int ret;

	/* Get device stats */
	if (fstat(dev->zbd_fd, &st) < 0) {
		ret = -errno;
		zbc_error("%s: stat failed %d (%s)\n",
			  dev->zbd_filename,
			  errno, strerror(errno));
		return ret;
	}

	if (S_ISBLK(st.st_mode)) {

		/* Get logical block size */
		ret = ioctl(dev->zbd_fd, BLKSSZGET, &size32);
		if (ret != 0) {
			ret = -errno;
			zbc_error("%s: ioctl BLKSSZGET failed %d (%s)\n",
				  dev->zbd_filename,
				  errno, strerror(errno));
			return ret;
		}

		dev_info->zbd_lblock_size = size32;
		if (!dev_info->zbd_lblock_size) {
			zbc_error("%s: invalid logical sector size %d\n",
				  dev->zbd_filename, size32);
			return -EINVAL;
		}

		/* Get physical block size */
		ret = ioctl(dev->zbd_fd, BLKPBSZGET, &size32);
		if (ret != 0) {
			ret = -errno;
			zbc_error("%s: ioctl BLKPBSZGET failed %d (%s)\n",
				  dev->zbd_filename,
				  errno, strerror(errno));
			return ret;
		}
		dev_info->zbd_pblock_size = size32;
		if (!dev_info->zbd_pblock_size) {
			zbc_error("%s: invalid physical sector size %d\n",
				  dev->zbd_filename,
				  size32);
			return -EINVAL;
		}

		/* Get capacity (B) */
		ret = ioctl(dev->zbd_fd, BLKGETSIZE64, &size64);
		if (ret != 0) {
			ret = -errno;
			zbc_error("%s: ioctl BLKGETSIZE64 failed %d (%s)\n",
				  dev->zbd_filename,
				  errno,
				  strerror(errno));
			return ret;
		}

		dev_info->zbd_pblocks = size64 / dev_info->zbd_pblock_size;

	} else if (S_ISREG(st.st_mode)) {

		/* Default value for files */
		if (st.st_blksize == 512 || st.st_blksize == 4096)
			dev_info->zbd_pblock_size = st.st_blksize;
		else
			dev_info->zbd_pblock_size = ZBC_FAKE_FILE_BLOCK_SIZE;
		dev_info->zbd_pblocks = st.st_size / dev_info->zbd_pblock_size;
		dev_info->zbd_lblock_size = ZBC_FAKE_FILE_BLOCK_SIZE;

	} else {

		return -ENXIO;

	}

	dev_info->zbd_lblocks =
		(dev_info->zbd_pblocks * dev_info->zbd_pblock_size) /
		dev_info->zbd_lblock_size;

	/* Check */
	if (!dev_info->zbd_lblocks) {
		zbc_error("%s: invalid capacity (logical blocks)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	if (!dev_info->zbd_pblocks) {
		zbc_error("%s: invalid capacity (physical blocks)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Finish setting */
	dev_info->zbd_type = ZBC_DT_FAKE;
	dev_info->zbd_model = ZBC_DM_HOST_MANAGED;
	strncpy(dev_info->zbd_vendor_id, "FAKE HGST HM libzbc",
		ZBC_DEVICE_INFO_LENGTH - 1);

	dev_info->zbd_sectors =
		(dev_info->zbd_lblock_size * dev_info->zbd_lblocks) >> 9;
	dev_info->zbd_opt_nr_open_seq_pref = 0;
	dev_info->zbd_opt_nr_non_seq_write_seq_pref = 0;
	dev_info->zbd_max_nr_open_seq_req = ZBC_FAKE_MAX_OPEN_NR_ZONES;

	/* Get maximum command size */
	zbc_sg_get_max_cmd_blocks(dev);

	return 0;
}

/**
 * zbc_fake_open - Open an emulation device or file.
 */
static int zbc_fake_open(const char *filename, int flags,
			 struct zbc_device **pdev)
{
	struct zbc_fake_device *fdev;
	int fd, ret;

	zbc_debug("%s: ########## Trying FAKE driver ##########\n",
		  filename);

	/* Open emulation device/file */
	fd = open(filename, flags | O_LARGEFILE);
	if (fd < 0) {
		ret = -errno;
		zbc_error("%s: open failed %d (%s)\n",
			  filename,
			  errno, strerror(errno));
		return ret;
	}

	/* Allocate a handle */
	ret = -ENOMEM;
	fdev = calloc(1, sizeof(*fdev));
	if (!fdev)
		goto out;

	fdev->dev.zbd_fd = fd;
	fdev->zbd_meta_fd = -1;
#ifdef HAVE_DEVTEST
	fdev->dev.zbd_o_flags = flags & ZBC_O_DEVTEST;
#endif

	fdev->dev.zbd_filename = strdup(filename);
	if (!fdev->dev.zbd_filename)
		goto out_free_dev;

	/* Set the fake device information */
	ret = zbc_fake_set_info(&fdev->dev);
	if (ret != 0)
		goto out_free_filename;

	/* Open metadata */
	ret = zbc_fake_open_metadata(fdev, flags & ZBC_O_SETZONES);
	if (ret != 0)
		goto out_free_filename;

	*pdev = &fdev->dev;

	zbc_debug("%s: ########## FAKE driver succeeded ##########\n",
		  filename);

	return 0;

out_free_filename:
	free(fdev->dev.zbd_filename);

out_free_dev:
	free(fdev);

out:
	close(fd);

	zbc_debug("%s: ########## FAKE driver failed %d ##########\n",
		  filename, ret);

	return ret;
}

/**
 * zbc_fake_close - Close a device.
 */
static int zbc_fake_close(struct zbc_device *dev)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);

	/* Close metadata */
	zbc_fake_close_metadata(fdev);

	/* Close device */
	close(dev->zbd_fd);

	free(dev->zbd_filename);
	free(dev);

	return 0;
}

/**
 * zbc_fake_must_report_zone - Test if a zone must be reported.
 */
static bool zbc_fake_must_report_zone(struct zbc_zone *zone,
				      uint64_t start_sector,
				      enum zbc_reporting_options ro)
{
	enum zbc_reporting_options options = ro & (~ZBC_RO_PARTIAL);

	if (zone->zbz_length == 0 ||
	    zone->zbz_start + zone->zbz_length <= start_sector)
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

/**
 * zbc_fake_report_zones - Get fake device zone information.
 */
static int zbc_fake_report_zones(struct zbc_device *dev, uint64_t sector,
				 enum zbc_reporting_options ro,
				 struct zbc_zone *zones, unsigned int *nr_zones)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	unsigned int max_nr_zones = *nr_zones;
	enum zbc_reporting_options options = ro & (~ZBC_RO_PARTIAL);
	unsigned int in, out = 0;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY, ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	/* Check reporting option */
	if (options != ZBC_RO_ALL &&
	    options != ZBC_RO_EMPTY &&
	    options != ZBC_RO_IMP_OPEN &&
	    options != ZBC_RO_EXP_OPEN &&
	    options != ZBC_RO_CLOSED &&
	    options != ZBC_RO_FULL &&
	    options != ZBC_RO_RDONLY &&
	    options != ZBC_RO_OFFLINE &&
	    options != ZBC_RO_RWP_RECOMMENDED &&
	    options != ZBC_RO_NON_SEQ &&
	    options != ZBC_RO_NOT_WP) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		return -EIO;
	}

	/* Check sector */
	if (sector >= dev->zbd_info.zbd_sectors) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		return -EIO;
	}

	zbc_fake_lock(fdev);

	if (!zones)
		max_nr_zones = fdev->zbd_nr_zones;

	/* Get matching zones */
	for (in = 0; in < fdev->zbd_nr_zones; in++) {
		if (zbc_fake_must_report_zone(&fdev->zbd_zones[in],
					      sector, options)) {
			if (zones && (out < max_nr_zones))
				memcpy(&zones[out], &fdev->zbd_zones[in],
				       sizeof(struct zbc_zone));
			out++;
		}
		if (out >= max_nr_zones && (ro & ZBC_RO_PARTIAL))
			break;
	}

	if (out > max_nr_zones)
		out = max_nr_zones;
	*nr_zones = out;

	zbc_fake_unlock(fdev);

	return 0;
}

/**
 * zbc_zone_do_close - Close a zone.
 */
static void zbc_zone_do_close(struct zbc_fake_device *fdev,
			      struct zbc_zone *zone)
{
	if (!zbc_zone_is_open(zone))
		return;

	if (zbc_zone_imp_open(zone))
		fdev->zbd_meta->zbd_nr_imp_open_zones--;
	else if (zbc_zone_exp_open(zone))
		fdev->zbd_meta->zbd_nr_exp_open_zones--;

	if (zone->zbz_write_pointer == zone->zbz_start)
		zone->zbz_condition = ZBC_ZC_EMPTY;
	else
		zone->zbz_condition = ZBC_ZC_CLOSED;
}

/**
 * zbc_fake_open_zone - Open zone(s).
 */
static int zbc_fake_open_zone(struct zbc_device *dev, uint64_t sector,
			      unsigned int flags)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone;
	unsigned int i;
	int ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY, ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	if (flags & ZBC_OP_ALL_ZONES) {

		unsigned int need_open = 0;

		/* Check if all closed zones can be open */
		for (i = 0; i < fdev->zbd_nr_zones; i++) {
			if (zbc_zone_closed(&fdev->zbd_zones[i]))
				need_open++;
		}
		if ((fdev->zbd_meta->zbd_nr_exp_open_zones + need_open) >
		    fdev->dev.zbd_info.zbd_max_nr_open_seq_req) {
			zbc_set_errno(ZBC_SK_DATA_PROTECT,
				      ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES);
			goto out;
		}

		/* Open all closed zones */
		for (i = 0; i < fdev->zbd_nr_zones; i++) {
			if (zbc_zone_closed(&fdev->zbd_zones[i]))
				fdev->zbd_zones[i].zbz_condition =
					ZBC_ZC_EXP_OPEN;
		}
		fdev->zbd_meta->zbd_nr_exp_open_zones += need_open;

		ret = 0;
		goto out;
	}

	/* Check sector */
	if (sector >= dev->zbd_info.zbd_sectors) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		goto out;
	}

	/* Check target zone */
	zone = zbc_fake_find_zone(fdev, sector, true);
	if (!zone) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_conventional(zone)) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	/* Full zone or already opened zone is being open: do nothing */
	if (zbc_zone_full(zone) || zbc_zone_exp_open(zone)) {
		ret = 0;
		goto out;
	}

	if (!(zbc_zone_closed(zone) ||
	      zbc_zone_imp_open(zone) ||
	      zbc_zone_empty(zone))) {
		goto out;
	}

	if (zbc_zone_imp_open(zone))
		zbc_zone_do_close(fdev, zone);

	/* Check limit */
	if ((fdev->zbd_meta->zbd_nr_exp_open_zones +
	     fdev->zbd_meta->zbd_nr_imp_open_zones + 1)
	    > fdev->dev.zbd_info.zbd_max_nr_open_seq_req) {

		if (!fdev->zbd_meta->zbd_nr_imp_open_zones) {
			zbc_set_errno(ZBC_SK_DATA_PROTECT,
				      ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES);
			goto out;
		}

		/* Close an implicitely open zone */
		for (i = 0; i < fdev->zbd_nr_zones; i++) {
			if (zbc_zone_imp_open(&fdev->zbd_zones[i])) {
				zbc_zone_do_close(fdev, &fdev->zbd_zones[i]);
				break;
			}
		}

	}

	/* Open the specified zone */
	zone->zbz_condition = ZBC_ZC_EXP_OPEN;
	fdev->zbd_meta->zbd_nr_exp_open_zones++;
	ret = 0;

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_zone_close_allowed - Test if a zone can be closed.
 */
static bool zbc_zone_close_allowed(struct zbc_zone *zone)
{
	return zbc_zone_sequential(zone) &&
		(zbc_zone_empty(zone) ||
		 zbc_zone_full(zone) ||
		 zbc_zone_imp_open(zone) ||
		 zbc_zone_exp_open(zone));
}

/**
 * zbc_fake_close_zone - Close zone(s).
 */
static int zbc_fake_close_zone(struct zbc_device *dev, uint64_t sector,
			       unsigned int flags)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone;
	unsigned int i;
	int ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY,
			      ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	if (flags & ZBC_OP_ALL_ZONES) {
		/* Close all open zones */
		for (i = 0; i < fdev->zbd_nr_zones; i++) {
			if (zbc_zone_close_allowed(&fdev->zbd_zones[i]))
				zbc_zone_do_close(fdev, &fdev->zbd_zones[i]);
		}
		ret = 0;
		goto out;
	}

	/* Check sector */
	if (sector >= dev->zbd_info.zbd_sectors) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		goto out;
	}

	/* Close the specified zone */
	zone = zbc_fake_find_zone(fdev, sector, true);
	if (!zone) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_conventional(zone)) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_close_allowed(zone)) {
		zbc_zone_do_close(fdev, zone);
		ret = 0;
	} else if (zbc_zone_closed(zone))
		ret = 0;
	else {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
	}

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_zone_finish_allowed - Test if a zone can be finished.
 */
static bool zbc_zone_finish_allowed(struct zbc_zone *zone)
{
	return zbc_zone_sequential(zone) &&
		(zbc_zone_imp_open(zone) ||
		 zbc_zone_exp_open(zone) ||
		 zbc_zone_closed(zone));
}

/**
 * zbc_zone_do_finish - Finish a zone.
 */
static void zbc_zone_do_finish(struct zbc_fake_device *fdev,
			       struct zbc_zone *zone)
{
	if (zbc_zone_is_open(zone))
		zbc_zone_do_close(fdev, zone);

	zone->zbz_write_pointer = (uint64_t)-1;
	zone->zbz_condition = ZBC_ZC_FULL;
}

/**
 * zbc_fake_finish_zone - Finish zone(s).
 */
static int zbc_fake_finish_zone(struct zbc_device *dev, uint64_t sector,
				unsigned int flags)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone;
	unsigned int i;
	int ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY,
			      ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	if (flags & ZBC_OP_ALL_ZONES) {
		/* Finish all open and closed zones */
		for (i = 0; i < fdev->zbd_nr_zones; i++) {
			if (zbc_zone_finish_allowed(&fdev->zbd_zones[i]))
				zbc_zone_do_finish(fdev, &fdev->zbd_zones[i]);
		}
		ret = 0;
		goto out;
	}

	/* Check sector */
	if (sector >= dev->zbd_info.zbd_sectors) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		goto out;
	}

	/* Finish the specified zone */
	zone = zbc_fake_find_zone(fdev, sector, true);
	if (!zone) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_conventional(zone)) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_finish_allowed(zone) || zbc_zone_empty(zone)) {
		zbc_zone_do_finish(fdev, zone);
		ret = 0;
	} else if (zbc_zone_full(zone))
		ret = 0;
	else {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
	}

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_zone_reset_allowed - Test if a zone write pointer can be reset.
 */
static bool zbc_zone_reset_allowed(struct zbc_zone *zone)
{
	return zbc_zone_sequential(zone) &&
		(zbc_zone_imp_open(zone) ||
		 zbc_zone_exp_open(zone) ||
		 zbc_zone_closed(zone) ||
		 zbc_zone_empty(zone) ||
		 zbc_zone_full(zone));
}

/**
 * zbc_zone_do_reset - Reset a zone write pointer.
 */
static void zbc_zone_do_reset(struct zbc_fake_device *fdev,
			      struct zbc_zone *zone)
{
	if (zbc_zone_empty(zone))
		return;

	if (zbc_zone_is_open(zone))
		zbc_zone_do_close(fdev, zone);

	zone->zbz_write_pointer = zone->zbz_start;
	zone->zbz_condition = ZBC_ZC_EMPTY;
}

/**
 * zbc_fake_reset_zone - Reset zone(s) write pointer.
 */
static int zbc_fake_reset_zone(struct zbc_device *dev, uint64_t sector,
			       unsigned int flags)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone;
	unsigned int i;
	int ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY,
			      ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	if (flags & ZBC_OP_ALL_ZONES) {
		/* Reset all open, closed and full zones */
		for (i = 0; i < fdev->zbd_nr_zones; i++) {
			if (zbc_zone_reset_allowed(&fdev->zbd_zones[i]))
				zbc_zone_do_reset(fdev, &fdev->zbd_zones[i]);
		}
		ret = 0;
		goto out;
	}

	/* Check sector */
	if (sector >= dev->zbd_info.zbd_sectors) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		goto out;
	}

	/* Reset the specified zone */
	zone = zbc_fake_find_zone(fdev, sector, true);
	if (!zone) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_conventional(zone)) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
		goto out;
	}

	if (zbc_zone_reset_allowed(zone)) {
		zbc_zone_do_reset(fdev, zone);
		ret = 0;
	} else if (zbc_zone_empty(zone))
		ret = 0;
	else {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_INVALID_FIELD_IN_CDB);
	}

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_fake_zone_op - Execute a zone operation.
 */
static int
zbc_fake_zone_op(struct zbc_device *dev, uint64_t sector,
		 enum zbc_zone_op op, unsigned int flags)
{
	switch (op) {
	case ZBC_OP_RESET_ZONE:
		return zbc_fake_reset_zone(dev, sector, flags);
	case ZBC_OP_OPEN_ZONE:
		return zbc_fake_open_zone(dev, sector, flags);
	case ZBC_OP_CLOSE_ZONE:
		return zbc_fake_close_zone(dev, sector, flags);
	case ZBC_OP_FINISH_ZONE:
		return zbc_fake_finish_zone(dev, sector, flags);
	default:
		return -EINVAL;
	}
}

/**
 * zbc_fake_pread - Read from the emulated device/file.
 */
static ssize_t zbc_fake_pread(struct zbc_device *dev, void *buf,
			      size_t count, uint64_t offset)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone;
	size_t nr_sectors;
	ssize_t ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY,
			      ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	/* Find the zone containing offset */
	zone = zbc_fake_find_zone(fdev, offset, false);
	if (!zone) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		goto out;
	}

	/*
	 * We are simulated a host-managed device with restricted reads
	 * so check the access alignement against zones and zone write pointer.
	 */
	nr_sectors = offset + count - zbc_zone_start(zone);

	if (zbc_zone_conventional(zone)) {

		/*
		 * Reading accross conventional zones is OK.
		 */
		while (nr_sectors > zbc_zone_length(zone)) {

			nr_sectors -= zbc_zone_length(zone);

			zone = zbc_fake_find_zone(fdev,
						  zbc_zone_start(zone) +
						  zbc_zone_length(zone),
						  true);
			if (!zone) {
				zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				    ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
				goto out;
			}

			if (!zbc_zone_conventional(zone)) {
				zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
					ZBC_ASC_ATTEMPT_TO_READ_INVALID_DATA);
				goto out;
			}

		}

	} else {

		/*
		 * Reading after the zone write pointer or
		 * accross zones is not allowed.
		 */
		if (nr_sectors > zbc_zone_length(zone)) {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				      ZBC_ASC_READ_BOUNDARY_VIOLATION);
			goto out;
		}

		if (nr_sectors > zbc_zone_wp(zone) - zbc_zone_start(zone)) {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				      ZBC_ASC_ATTEMPT_TO_READ_INVALID_DATA);
			goto out;
		}

	}

	/* Do read */
	ret = pread(dev->zbd_fd, buf, count << 9, offset << 9);
	if (ret < 0) {
		zbc_set_errno(ZBC_SK_MEDIUM_ERROR,
			      ZBC_ASC_READ_ERROR);
		ret = -errno;
	} else
		ret >>= 9;

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_fake_pwrite - Write to the emulated device/file.
 */
static ssize_t zbc_fake_pwrite(struct zbc_device *dev, const void *buf,
			       size_t count, uint64_t offset)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone, *next_zone;
	uint64_t next_sector;
	ssize_t ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY,
			      ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	/* Find the target zone */
	zone = zbc_fake_find_zone(fdev, offset, false);
	if (!zone) {
		zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
			      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		goto out;
	}

	/* Write cannot span zones */
	next_sector = zbc_zone_start(zone) + zbc_zone_length(zone);
	next_zone = zbc_fake_find_zone(fdev, next_sector, true);
	if (offset + count > next_sector) {
		if (next_zone) {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				      ZBC_ASC_WRITE_BOUNDARY_VIOLATION);
		} else {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
		}
		goto out;
	}

	if (zbc_zone_sequential_req(zone)) {

		/* Cannot write a full zone */
		if (zbc_zone_full(zone)) {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				      ZBC_ASC_INVALID_FIELD_IN_CDB);
			goto out;
		}

		/* Can only write at the write pointer */
		if (offset != zbc_zone_wp(zone)) {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				      ZBC_ASC_UNALIGNED_WRITE_COMMAND);
			goto out;
		}

		/* Writes must be aligned on the physical block size */
		if (!zbc_dev_sect_paligned(dev, count) ||
		    !zbc_dev_sect_paligned(dev, offset)) {
			zbc_set_errno(ZBC_SK_ILLEGAL_REQUEST,
				      ZBC_ASC_UNALIGNED_WRITE_COMMAND);
			goto out;
		}

		/* Can only write an open zone */
		if (!zbc_zone_is_open(zone)) {

			if (fdev->zbd_meta->zbd_nr_exp_open_zones >=
			    fdev->dev.zbd_info.zbd_max_nr_open_seq_req) {
				/* Too many explicit open on-going */
				zbc_set_errno(ZBC_SK_DATA_PROTECT,
					ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES);
				goto out;
			}

			/* Implicitly open the zone */
			if (fdev->zbd_meta->zbd_nr_imp_open_zones >=
			    fdev->dev.zbd_info.zbd_max_nr_open_seq_req) {
				struct zbc_zone *z = fdev->zbd_zones;
				unsigned int i;

				for (i = 0; i < fdev->zbd_nr_zones; i++, z++) {
					if (zbc_zone_imp_open(z)) {
						zbc_zone_do_close(fdev, z);
						break;
					}
				}
			}

			zone->zbz_condition = ZBC_ZC_IMP_OPEN;
			fdev->zbd_meta->zbd_nr_imp_open_zones++;

		}

	}

	/* Do write */
	ret = pwrite(dev->zbd_fd, buf, count << 9, offset << 9);
	if (ret < 0) {
		zbc_set_errno(ZBC_SK_MEDIUM_ERROR, ZBC_ASC_WRITE_ERROR);
		ret = -errno;
		goto out;
	}

	ret >>= 9;

	if (zbc_zone_sequential_req(zone)) {
		/* Advance write pointer */
		zone->zbz_write_pointer += ret;
		if (zone->zbz_write_pointer >= next_sector) {
			if (zbc_zone_imp_open(zone))
				fdev->zbd_meta->zbd_nr_imp_open_zones--;
			else
				fdev->zbd_meta->zbd_nr_exp_open_zones--;
			zone->zbz_condition = ZBC_ZC_FULL;
		}
	}

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_fake_flush - Flush the emulated device data and metadata.
 */
static int zbc_fake_flush(struct zbc_device *dev)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	int ret;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY, ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	ret = msync(fdev->zbd_meta, fdev->zbd_meta_size, MS_SYNC);
	if (ret == 0)
		ret = fsync(dev->zbd_fd);

	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * zbc_fake_set_zones - Initialize an emulated device metadata.
 */
static int zbc_fake_set_zones(struct zbc_device *dev,
			      uint64_t conv_sz, uint64_t zone_sz)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	uint64_t sector = 0, device_size = dev->zbd_info.zbd_sectors;
	uint64_t capacity_bytes;
	struct zbc_fake_meta fmeta;
	char meta_path[ZBC_FAKE_META_PATH_SIZE];
	struct zbc_zone *zone;
	unsigned int z = 0;
	int ret;

	/* Initialize metadata */
	if (fdev->zbd_meta)
		zbc_fake_close_metadata(fdev);

	memset(&fmeta, 0, sizeof(struct zbc_fake_meta));

	/* Calculate zone configuration */
	if (conv_sz + zone_sz > device_size) {
		zbc_error("%s: invalid zone sizes (too large)\n",
			  fdev->dev.zbd_filename);
		return -EINVAL;
	}

	fmeta.zbd_nr_conv_zones = conv_sz / zone_sz;
	if (conv_sz &&
	    !fmeta.zbd_nr_conv_zones)
		fmeta.zbd_nr_conv_zones = 1;

	fmeta.zbd_nr_seq_zones =
		(device_size - (fmeta.zbd_nr_conv_zones * zone_sz)) / zone_sz;
	if (!fmeta.zbd_nr_seq_zones) {
		zbc_error("%s: invalid zone sizes (too large)\n",
			  fdev->dev.zbd_filename);
		return -EINVAL;
	}

	fmeta.zbd_nr_zones = fmeta.zbd_nr_conv_zones + fmeta.zbd_nr_seq_zones;
	fdev->zbd_nr_zones = fmeta.zbd_nr_zones;

	dev->zbd_info.zbd_sectors = fdev->zbd_nr_zones * zone_sz;
	capacity_bytes = dev->zbd_info.zbd_sectors << 9;

	dev->zbd_info.zbd_lblocks =
		capacity_bytes / dev->zbd_info.zbd_lblock_size;
	dev->zbd_info.zbd_pblocks =
		capacity_bytes / dev->zbd_info.zbd_pblock_size;
	fmeta.zbd_capacity = dev->zbd_info.zbd_lblocks *
		dev->zbd_info.zbd_lblock_size;

	/* Open metadata file */
	zbc_fake_dev_meta_path(fdev, meta_path);
	fdev->zbd_meta_fd = open(meta_path, O_RDWR | O_CREAT, 0600);
	if (fdev->zbd_meta_fd < 0) {
		ret = -errno;
		zbc_error("%s: open metadata file %s failed %d (%s)\n",
			  fdev->dev.zbd_filename,
			  meta_path,
			  errno, strerror(errno));
		return ret;
	}

	/* Truncate metadata file */
	fdev->zbd_meta_size = sizeof(struct zbc_fake_meta) +
		(fdev->zbd_nr_zones * sizeof(struct zbc_zone));
	if (ftruncate(fdev->zbd_meta_fd, fdev->zbd_meta_size) < 0) {
		ret = -errno;
		zbc_error("%s: truncate meta file %s to %zu B failed %d (%s)\n",
			  fdev->dev.zbd_filename,
			  meta_path,
			  fdev->zbd_meta_size,
			  errno, strerror(errno));
		goto out;
	}

	/* mmap metadata file */
	fdev->zbd_meta = mmap(NULL, fdev->zbd_meta_size,
			      PROT_READ | PROT_WRITE, MAP_SHARED,
			      fdev->zbd_meta_fd, 0);
	if (fdev->zbd_meta == MAP_FAILED) {
		fdev->zbd_meta = NULL;
		zbc_error("%s: mmap metadata file %s failed\n",
			  fdev->dev.zbd_filename,
			  meta_path);
		ret = -ENOMEM;
		goto out;
	}

	fdev->zbd_zones = (struct zbc_zone *) (fdev->zbd_meta + 1);

	/* Setup metadata header */
	memcpy(fdev->zbd_meta, &fmeta, sizeof(struct zbc_fake_meta));

	/* Setup conventional zones descriptors */
	for (z = 0; z < fmeta.zbd_nr_conv_zones; z++) {

		zone = &fdev->zbd_zones[z];

		zone->zbz_type = ZBC_ZT_CONVENTIONAL;
		zone->zbz_condition = ZBC_ZC_NOT_WP;
		zone->zbz_start = sector;
		zone->zbz_write_pointer = (uint64_t)-1;
		zone->zbz_length = zone_sz;

		memset(&zone->__pad, 0, sizeof(zone->__pad));

		sector += zone_sz;

	}

	/* Setup sequential zones descriptors */
	for (; z < fdev->zbd_nr_zones; z++) {

		zone = &fdev->zbd_zones[z];

		zone->zbz_type = ZBC_ZT_SEQUENTIAL_REQ;
		zone->zbz_condition = ZBC_ZC_EMPTY;
		zone->zbz_start = sector;
		zone->zbz_write_pointer = zone->zbz_start;
		zone->zbz_length = zone_sz;

		memset(&zone->__pad, 0, sizeof(zone->__pad));

		sector += zone_sz;

	}

	ret = 0;

out:
	if (ret != 0)
		zbc_fake_close_metadata(fdev);

	return ret;
}

/**
 * zbc_fake_set_write_pointer - Change the value of a zone write pointer.
 */
static int zbc_fake_set_write_pointer(struct zbc_device *dev,
				      uint64_t sector, uint64_t wp_sector)
{
	struct zbc_fake_device *fdev = zbc_fake_to_file_dev(dev);
	struct zbc_zone *zone;
	int ret = -EIO;

	if (!fdev->zbd_meta) {
		zbc_set_errno(ZBC_SK_NOT_READY, ZBC_ASC_FORMAT_IN_PROGRESS);
		return -ENXIO;
	}

	zbc_fake_lock(fdev);

	zone = zbc_fake_find_zone(fdev, sector, true);
	if (!zone)
		goto out;

	/* Do nothing for conventional zones */
	if (zbc_zone_sequential_req(zone)) {

		if (zbc_zone_is_open(zone))
			zbc_zone_do_close(fdev, zone);

		zone->zbz_write_pointer = wp_sector;
		if (zone->zbz_write_pointer == zone->zbz_start) {
			zone->zbz_condition = ZBC_ZC_EMPTY;
		} else if (zone->zbz_write_pointer > zone->zbz_start &&
			   zone->zbz_write_pointer <
			   zone->zbz_start + zone->zbz_length) {
			zone->zbz_condition = ZBC_ZC_CLOSED;
		} else {
			zone->zbz_condition = ZBC_ZC_FULL;
			zone->zbz_write_pointer = (uint64_t)-1;
		}

	}

	ret = 0;

out:
	zbc_fake_unlock(fdev);

	return ret;
}

/**
 * Fake backend driver definition.
 */
struct zbc_drv zbc_fake_drv = {
	.flag			= ZBC_O_DRV_FAKE,
	.zbd_open		= zbc_fake_open,
	.zbd_close		= zbc_fake_close,
	.zbd_pread		= zbc_fake_pread,
	.zbd_pwrite		= zbc_fake_pwrite,
	.zbd_flush		= zbc_fake_flush,
	.zbd_report_zones	= zbc_fake_report_zones,
	.zbd_zone_op		= zbc_fake_zone_op,
	.zbd_set_zones		= zbc_fake_set_zones,
	.zbd_set_wp		= zbc_fake_set_write_pointer,
};
