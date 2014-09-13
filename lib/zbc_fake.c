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
 * Author: Christoph Hellwig (hch@infradead.org)
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <linux/fs.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "zbc.h"

static struct zbc_zone *
zbf_file_find_zone(struct zbc_device *dev, uint64_t zone_start_lba)
{
        int i;

        for (i = 0; i < dev->zbd_nr_zones; i++)
                if (dev->zbd_zones[i].zbz_start == zone_start_lba)
                        return &dev->zbd_zones[i];
        return NULL;
}

static int
zbc_file_open_metadata(struct zbc_device *dev)
{
        char meta_path[512];
        struct stat st;
        int error;
        int i;

        sprintf(meta_path, "/tmp/zbc-%s.meta", basename(dev->zbd_filename));

        zbc_debug("Device %s: using meta file %s\n",
                  dev->zbd_filename,
                  meta_path);

        dev->zbd_meta_fd = open(meta_path, O_RDWR);
        if (dev->zbd_meta_fd < 0) {
                /*
                 * Metadata didn't exist yet, we'll have to wait for a set_zones
                 * call.
                 */
		if (errno == ENOENT)
	                return 0;
                perror("open metadata");
		error = -errno;
		goto out_close;
        }

        if (fstat(dev->zbd_meta_fd, &st) < 0) {
                perror("fstat");
                error = -errno;
                goto out_close;
        }

        dev->zbd_zones = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, dev->zbd_meta_fd, 0);
        if (dev->zbd_zones == MAP_FAILED) {
                perror("mmap\n");
                error = -ENOMEM;
                goto out_close;
        }

        for (i = 0; dev->zbd_zones[i].zbz_length != 0; i++)
                ;

        dev->zbd_nr_zones = i;
        return 0;
out_close:
        close(dev->zbd_meta_fd);
        return error;
}

/**
 * Default to regular sector size for emulation on top of a regular file.
 */
#define ZBC_FILE_SECTOR_SIZE    512

static int
zbc_file_get_info(struct zbc_device *dev, struct stat *st)
{
	dev->zbd_info.zbd_logical_block_size = ZBC_FILE_SECTOR_SIZE; 
	dev->zbd_info.zbd_logical_blocks = st->st_size / ZBC_FILE_SECTOR_SIZE;
	dev->zbd_info.zbd_physical_block_size = dev->zbd_info.zbd_logical_block_size;
	dev->zbd_info.zbd_physical_blocks = dev->zbd_info.zbd_logical_blocks;
	return 0;
}

/**
 * Get a block device information (capacity & sector sizes).
 */
static int
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

static int
zbc_file_open(const char *filename, int flags, struct zbc_device **pdev)
{
	struct zbc_device *dev;
	struct stat st;
	int fd, ret;

	fd = open(filename, flags);
	if (fd < 0) {
		zbc_error("Open device file %s failed %d (%s)\n",
			filename,
			errno,
			strerror(errno));
		return -errno;
	}

	if (fstat(fd, &st) < 0) {
		zbc_error("Stat device %s failed %d (%s)\n",
			filename,
			errno,
			strerror(errno));
		ret = -errno;
		goto out;
	}


	/* Set device operation */
	if (!S_ISBLK(st.st_mode) && !S_ISREG(st.st_mode)) {
		ret = -ENXIO;
		goto out;
	}

	dev = zbc_dev_alloc(filename, flags);
	if (!dev) {
		ret = -ENOMEM;
		goto out;
	}

	dev->zbd_fd = fd;
	dev->zbd_flags = flags;

	dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
	if (S_ISBLK(st.st_mode))
		ret = zbc_blkdev_get_info(dev);
	else
		ret = zbc_file_get_info(dev, &st);

	if (ret)
		goto out_free_dev;

	ret = zbc_file_open_metadata(dev);
	if (ret)
		goto out_free_dev;

	*pdev = dev;
	return 0;

out_free_dev:
	zbc_dev_free(dev);
out:
	close(fd);
	return ret;
}

static int
zbc_file_close(zbc_device_t *dev)
{
	int ret = 0;

	if (dev->zbd_meta_fd != -1) {
		if (close(dev->zbd_meta_fd) < 0)
			ret = -errno;
	}
        if (close(dev->zbd_fd) < 0) {
		if (!ret)
			ret = -errno;
	}

	if (!ret)
		zbc_dev_free(dev);
	return ret;
}

static bool
want_zone(struct zbc_zone *zone, uint64_t start_lba,
                enum zbc_reporting_options options)
{
        if (zone->zbz_length == 0)
                return false;
        if (zone->zbz_start < start_lba)
                return false;

        switch (options) {
        case ZBC_RO_ALL:
                return true;
        case ZBC_RO_FULL:
                return zone->zbz_condition == ZBC_ZC_FULL;
        case ZBC_RO_OPEN:
                return zone->zbz_condition == ZBC_ZC_OPEN;
        case ZBC_RO_EMPTY:
                return zone->zbz_condition == ZBC_ZC_EMPTY;
        case ZBC_RO_RDONLY:
                return zone->zbz_condition == ZBC_ZC_RDONLY;
        case ZBC_RO_OFFLINE:
                return zone->zbz_condition == ZBC_ZC_OFFLINE;
        case ZBC_RO_RESET:
                return zone->zbz_need_reset;
        default:
                return false;
        }
}

static int
zbc_file_nr_zones(struct zbc_device *dev, uint64_t start_lba,
                  enum zbc_reporting_options options, unsigned int *nr_zones)
{
        int in, out;

        out = 0;
        for (in = 0; in < dev->zbd_nr_zones; in++) {
                if (want_zone(&dev->zbd_zones[in], start_lba, options))
                        out++;
        }

        *nr_zones = out;
        return 0;
}

static int
zbc_file_report_zones(struct zbc_device *dev, uint64_t start_lba,
                enum zbc_reporting_options options, struct zbc_zone *zones,
                unsigned int *nr_zones)
{
        unsigned int max_nr_zones = *nr_zones;
        int in, out;

        if (!dev->zbd_zones)
                return -ENXIO;

        if (!zones)
                return zbc_file_nr_zones(dev, start_lba, options, nr_zones);

        out = 0;
        for (in = 0; in < dev->zbd_nr_zones; in++) {
                if (want_zone(&dev->zbd_zones[in], start_lba, options)) {
                        memcpy(&zones[out], &dev->zbd_zones[in],
                                sizeof(struct zbc_zone));
                        if (++out == max_nr_zones)
                                break;
                }
        }

        *nr_zones = out;
        return 0;
}

static bool
zbc_zone_reset_allowed(struct zbc_zone *zone)
{
        switch (zone->zbz_type) {
        case ZBC_ZT_SEQUENTIAL_REQ:
        case ZBC_ZT_SEQUENTIAL_PREF:
                return zone->zbz_condition == ZBC_ZC_OPEN ||
                        zone->zbz_condition == ZBC_ZC_FULL;
        default:
                return false;
        }
}

static int
zbc_file_reset_one_write_pointer(struct zbc_zone *zone)
{
        if (!zbc_zone_reset_allowed(zone))
                return -EINVAL;
        zone->zbz_write_pointer = zone->zbz_start;
        zone->zbz_condition = ZBC_ZC_EMPTY;
        return 0;
}

static int
zbc_file_reset_wp(struct zbc_device *dev, uint64_t zone_start_lba)
{
        struct zbc_zone *zone;

        if (!dev->zbd_zones)
                return -ENXIO;

        if (zone_start_lba == (uint64_t)-1) {
                int i;

                for (i = 0; i < dev->zbd_nr_zones; i++)
                        zbc_file_reset_one_write_pointer(&dev->zbd_zones[i]);
        } else {
                zone = zbf_file_find_zone(dev, zone_start_lba);
                if (!zone)
                        return -EIO;

                /* XXX(hch): reject for conventional zones? */

                zbc_file_reset_one_write_pointer(zone);
        }

        return 0;
}
 
static int32_t
zbc_file_pread(struct zbc_device *dev, struct zbc_zone *zone, void *buf,
               uint32_t lba_count, uint64_t start_lba)
{
        off_t offset;
        size_t count, ret;

        if (!dev->zbd_zones)
                return -ENXIO;

        if (start_lba > zone->zbz_length)
                return -EIO;
        start_lba += zone->zbz_start;

        if (zone->zbz_write_pointer + lba_count >
            zone->zbz_start + zone->zbz_length)
                return -EIO;

        if (zone->zbz_type == ZBC_ZT_SEQUENTIAL_REQ) {
                if (start_lba + lba_count > zone->zbz_write_pointer)
                        return -EIO;
        }

        /* XXX: check for overflows */
        count = lba_count * dev->zbd_info.zbd_logical_block_size;
        offset = start_lba * dev->zbd_info.zbd_logical_block_size;

        ret = pread(dev->zbd_fd, buf, count, offset);
        if (ret < 0)
                return -errno;

        return ret / dev->zbd_info.zbd_logical_block_size;
}

static int32_t
zbc_file_pwrite(struct zbc_device *dev, struct zbc_zone *z, const void *buf,
                uint32_t lba_count, uint64_t start_lba)
{
        struct zbc_zone *zone;
        off_t offset;
        size_t count, ret;

        if (!dev->zbd_zones)
                return -ENXIO;

        zone = zbf_file_find_zone(dev, z->zbz_start);
        if (!zone)
                return -EIO;

        if (start_lba > zone->zbz_length)
                return -EIO;
        start_lba += zone->zbz_start;

        if (zone->zbz_type == ZBC_ZT_SEQUENTIAL_REQ) {
                if (start_lba != zone->zbz_write_pointer)
                        return -EIO;
        }

        if (zone->zbz_write_pointer + lba_count >
            zone->zbz_start + zone->zbz_length)
                return -EIO;

        /* XXX: check for overflows */
        count = lba_count * dev->zbd_info.zbd_logical_block_size;
        offset = start_lba * dev->zbd_info.zbd_logical_block_size;

        ret = pwrite(dev->zbd_fd, buf, count, offset);
        if (ret < 0)
                return -errno;

        ret /= dev->zbd_info.zbd_logical_block_size;

        /*
         * XXX: What protects us from a return value that's not LBA aligned?
         * (Except for hoping the OS implementation isn't insane..)
         */
        zone->zbz_write_pointer += ret;
        if (zone->zbz_write_pointer == zone->zbz_start + zone->zbz_length)
                zone->zbz_condition = ZBC_ZC_FULL;
        else
                zone->zbz_condition = ZBC_ZC_OPEN;

        memcpy(z, zone, sizeof(*z));
        return ret;
}

static int
zbc_file_flush(struct zbc_device *dev, uint64_t lba_offset, uint32_t lba_count,
                int immediate)
{
        return fsync(dev->zbd_fd);
}

static int
zbc_file_set_zones(struct zbc_device *dev, uint64_t conv_zone_size,
                   uint64_t seq_zone_size)
{
        char meta_path[512];
        struct stat st;
        off_t len;
        int error;
        off_t device_size, start = 0;
        int z = 0;

        if (fstat(dev->zbd_fd, &st) < 0) {
                perror("fstat");
                return -errno;
        }

        /* Convert device size into # of physical blocks */
        device_size = dev->zbd_info.zbd_logical_blocks;

        if (conv_zone_size + seq_zone_size > device_size) {
                printf("size: %llu + %llu > %llu\n",
                       (unsigned long long) conv_zone_size,
                       (unsigned long long) seq_zone_size,
                       (unsigned long long) device_size);
                return -EINVAL;
        }

        dev->zbd_nr_zones =
                (device_size - conv_zone_size + seq_zone_size - 1) /
                 seq_zone_size;
        if (conv_zone_size)
                dev->zbd_nr_zones++;

        len = (off_t)dev->zbd_nr_zones * sizeof(struct zbc_zone);

        if (dev->zbd_meta_fd < 0) {
                sprintf(meta_path, "/tmp/zbc-%s.meta", basename(dev->zbd_filename));
                dev->zbd_meta_fd = open(meta_path, O_RDWR | O_CREAT, 0600);
                if (dev->zbd_meta_fd < 0) {
                        perror("open");
                        return -errno;
                }
        }

        if (ftruncate(dev->zbd_meta_fd, len) < 0) {
                perror("ftruncate");
                error = -errno;
                goto out_close;
        }

        dev->zbd_zones = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
                        dev->zbd_meta_fd, 0);
        if (dev->zbd_zones == MAP_FAILED) {
                perror("mmap");
                error = -ENOMEM;
                goto out_close;
        }

        if (conv_zone_size) {
                struct zbc_zone *zone = &dev->zbd_zones[z];

                zone->zbz_type = ZBC_ZT_CONVENTIONAL;
                zone->zbz_condition = 0;
                zone->zbz_start = start;
                zone->zbz_write_pointer = 0;
                zone->zbz_length = conv_zone_size;
                zone->zbz_need_reset = false;
                memset(&zone->__pad, 0, sizeof(zone->__pad));

                start += conv_zone_size;
                z++;
        }

        for (; z < dev->zbd_nr_zones; z++) {
                struct zbc_zone *zone = &dev->zbd_zones[z];

                zone->zbz_type = ZBC_ZT_SEQUENTIAL_REQ;
                zone->zbz_condition = ZBC_ZC_EMPTY;

                zone->zbz_start = start;
                zone->zbz_write_pointer = zone->zbz_start;

                if (zone->zbz_start + seq_zone_size <= device_size)
                        zone->zbz_length = seq_zone_size;
                else
                        zone->zbz_length = device_size - zone->zbz_start;

                zone->zbz_need_reset = false;
                memset(&zone->__pad, 0, sizeof(zone->__pad));

                start += zone->zbz_length;
        }

        return 0;

out_close:
        close(dev->zbd_meta_fd);
        return error;
}

static int
zbc_file_set_write_pointer(struct zbc_device *dev, uint64_t start_lba,
                           uint64_t write_pointer)
{
        struct zbc_zone *zone;

        if (!dev->zbd_zones)
                return -ENXIO;

        zone = zbf_file_find_zone(dev, start_lba);
        if (!zone)
                return -EINVAL;

        /* XXX(hch): reject for conventional zones? */

        zone->zbz_write_pointer = write_pointer;
        if (zone->zbz_write_pointer == zone->zbz_start + zone->zbz_length)
                zone->zbz_condition = ZBC_ZC_FULL;
        else
                zone->zbz_condition = ZBC_ZC_OPEN;
        return 0;
}

struct zbc_ops zbc_file_ops = {
	.zbd_open			= zbc_file_open,
	.zbd_close			= zbc_file_close,
        .zbd_pread                      = zbc_file_pread,
        .zbd_pwrite                     = zbc_file_pwrite,
        .zbd_flush                      = zbc_file_flush,
        .zbd_report_zones               = zbc_file_report_zones,
        .zbd_reset_wp                   = zbc_file_reset_wp,
        .zbd_set_zones                  = zbc_file_set_zones,
        .zbd_set_wp                     = zbc_file_set_write_pointer,
};
