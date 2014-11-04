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

#include <pthread.h>

#include "zbc.h"

struct zbc_file_device {
        struct zbc_device dev;

	pthread_mutex_t mutex;
        unsigned int zbd_nr_zones;
        struct zbc_zone *zbd_zones;
        int zbd_meta_fd;

};

static inline struct zbc_file_device *to_file_dev(struct zbc_device *dev)
{
        return container_of(dev, struct zbc_file_device, dev);
}

static struct zbc_zone *
zbf_file_find_zone(struct zbc_file_device *fdev, uint64_t zone_start_lba)
{
        int i;

        for (i = 0; i < fdev->zbd_nr_zones; i++)
                if (fdev->zbd_zones[i].zbz_start == zone_start_lba)
                        return &fdev->zbd_zones[i];
        return NULL;
}

static int
zbc_file_open_metadata(struct zbc_file_device *fdev)
{
        char meta_path[512];
        struct stat st;
        int error;
        int i;

        sprintf(meta_path, "/tmp/zbc-%s.meta",
                basename(fdev->dev.zbd_filename));

        zbc_debug("Device %s: using meta file %s\n",
                  fdev->dev.zbd_filename,
                  meta_path);

        fdev->zbd_meta_fd = open(meta_path, O_RDWR);
        if (fdev->zbd_meta_fd < 0) {
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

        if (fstat(fdev->zbd_meta_fd, &st) < 0) {
                perror("fstat");
                error = -errno;
                goto out_close;
        }

        fdev->zbd_zones = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fdev->zbd_meta_fd, 0);
        if (fdev->zbd_zones == MAP_FAILED) {
                perror("mmap\n");
                error = -ENOMEM;
                goto out_close;
        }

        for (i = 0; fdev->zbd_zones[i].zbz_length != 0; i++)
                ;

        fdev->zbd_nr_zones = i;
        return 0;
out_close:
        close(fdev->zbd_meta_fd);
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
        struct zbc_file_device *fdev;
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
        ret = -ENXIO;
        if (!S_ISBLK(st.st_mode) && !S_ISREG(st.st_mode))
                goto out;

        ret = -ENOMEM;
        fdev = calloc(1, sizeof(*fdev));
        if (!fdev)
                goto out;

        fdev->dev.zbd_fd = fd;
        fdev->zbd_meta_fd = -1;
        fdev->dev.zbd_filename = strdup(filename);
        if (!fdev->dev.zbd_filename)
                goto out_free_dev;

        fdev->dev.zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
        if (S_ISBLK(st.st_mode))
                ret = zbc_blkdev_get_info(&fdev->dev);
        else
                ret = zbc_file_get_info(&fdev->dev, &st);

        if (ret)
                goto out_free_filename;

        ret = zbc_file_open_metadata(fdev);
        if (ret)
                goto out_free_filename;

	pthread_mutex_init(&fdev->mutex, NULL);

        *pdev = &fdev->dev;
        return 0;

out_free_filename:
        free(fdev->dev.zbd_filename);
out_free_dev:
        free(fdev);
out:
        close(fd);
        return ret;
}

static int
zbc_file_close(zbc_device_t *dev)
{
        struct zbc_file_device *fdev = to_file_dev(dev);
        int ret = 0;

        if (fdev->zbd_meta_fd != -1) {
                if (close(fdev->zbd_meta_fd) < 0)
                        ret = -errno;
        }
        if (close(dev->zbd_fd) < 0) {
                if (!ret)
                        ret = -errno;
        }

        if (!ret) {
                free(dev->zbd_filename);
                free(dev);
        }
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
                return zbc_zone_need_reset(zone);
        case ZBC_RO_NON_SEQ:
                return zbc_zone_non_seq(zone);
        case ZBC_RO_NOT_WP:
                return zone->zbz_condition == ZBC_ZC_NOT_WP;
        default:
                return false;
        }
}

static int
zbc_file_nr_zones(struct zbc_file_device *fdev, uint64_t start_lba,
                  enum zbc_reporting_options options, unsigned int *nr_zones)
{
        int in, out;

        out = 0;
        for (in = 0; in < fdev->zbd_nr_zones; in++) {
                if (want_zone(&fdev->zbd_zones[in], start_lba, options))
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
        struct zbc_file_device *fdev = to_file_dev(dev);
        unsigned int max_nr_zones = *nr_zones;
        int in, out, ret;

        if (!fdev->zbd_zones)
                return -ENXIO;

	pthread_mutex_lock(&fdev->mutex);
        if (!zones) {
                ret = zbc_file_nr_zones(fdev, start_lba, options, nr_zones);
		goto out_unlock;
	}

        out = 0;
        for (in = 0; in < fdev->zbd_nr_zones; in++) {
                if (want_zone(&fdev->zbd_zones[in], start_lba, options)) {
                        memcpy(&zones[out], &fdev->zbd_zones[in],
                                sizeof(struct zbc_zone));
                        if (++out == max_nr_zones)
                                break;
                }
        }

        *nr_zones = out;
	ret = 0;
out_unlock:
	pthread_mutex_unlock(&fdev->mutex);
        return ret;
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
        struct zbc_file_device *fdev = to_file_dev(dev);
        struct zbc_zone *zone;
	int ret;

        if (!fdev->zbd_zones)
                return -ENXIO;

	pthread_mutex_lock(&fdev->mutex);
        if (zone_start_lba == (uint64_t)-1) {
                int i;

                for (i = 0; i < fdev->zbd_nr_zones; i++)
                        zbc_file_reset_one_write_pointer(&fdev->zbd_zones[i]);
        } else {
                zone = zbf_file_find_zone(fdev, zone_start_lba);
                if (!zone) {
                        ret = -EIO;
			goto out_unlock;
		}

                /* XXX(hch): reject for conventional zones? */

                zbc_file_reset_one_write_pointer(zone);
        }

	ret = 0;
out_unlock:
	pthread_mutex_unlock(&fdev->mutex);
        return ret;
}

static int32_t
zbc_file_pread(struct zbc_device *dev, struct zbc_zone *zone, void *buf,
               uint32_t lba_count, uint64_t start_lba)
{
        struct zbc_file_device *fdev = to_file_dev(dev);
        off_t offset;
        size_t count, ret;

        if (!fdev->zbd_zones)
                return -ENXIO;

        ret = -EIO;
	pthread_mutex_lock(&fdev->mutex);
        if (start_lba > zone->zbz_length)
		goto out_unlock;
        start_lba += zone->zbz_start;

        /* Note: unrestricted read will be added to the standard */
        /* and supported by a drive if the URSWRZ bit is set in  */
        /* VPD page. So this test will need to change.           */
        if (zone->zbz_type == ZBC_ZT_SEQUENTIAL_REQ) {
                /* Cannot read unwritten data */
                if (start_lba + lba_count > zone->zbz_write_pointer)
			goto out_unlock;
        } else {
                /* Reads spanning other types of zones are OK. */
                if (start_lba + lba_count > zone->zbz_start + zone->zbz_length) {
                        uint64_t lba = zbc_zone_end_lba(zone) + 1;
                        uint64_t count = start_lba + lba_count - lba;
                        struct zbc_zone *next_zone = zone;
                        while( count && (next_zone = zbf_file_find_zone(fdev, lba)) ) {
                                if (next_zone->zbz_type == ZBC_ZT_SEQUENTIAL_REQ)
					goto out_unlock;
                                if ( count > next_zone->zbz_length )
                                        count -= next_zone->zbz_length;
                                lba += next_zone->zbz_length;
                        }
                }
        }
	pthread_mutex_unlock(&fdev->mutex);

        /* XXX: check for overflows */
        count = lba_count * dev->zbd_info.zbd_logical_block_size;
        offset = start_lba * dev->zbd_info.zbd_logical_block_size;

        ret = pread(dev->zbd_fd, buf, count, offset);
        if (ret < 0)
                return -errno;

        return ret / dev->zbd_info.zbd_logical_block_size;

out_unlock:
	pthread_mutex_unlock(&fdev->mutex);
        return ret;
}

static int32_t
zbc_file_pwrite(struct zbc_device *dev, struct zbc_zone *z, const void *buf,
                uint32_t lba_count, uint64_t start_lba)
{
        struct zbc_file_device *fdev = to_file_dev(dev);
        struct zbc_zone *zone;
        off_t offset;
        size_t count, ret;

        if (!fdev->zbd_zones)
                return -ENXIO;

	ret = -EIO;
	pthread_mutex_lock(&fdev->mutex);
        zone = zbf_file_find_zone(fdev, z->zbz_start);
        if (!zone)
                goto out_unlock;

        if (start_lba > zone->zbz_length)
                goto out_unlock;
        start_lba += zone->zbz_start;

        if (zone->zbz_type == ZBC_ZT_SEQUENTIAL_REQ) {
                /* Can only write at the write pointer */
                if (start_lba != zone->zbz_write_pointer)
                        goto out_unlock;
        }

        /* Writes cannot span zones */
        if (zone->zbz_type != ZBC_ZT_CONVENTIONAL) {
                if (zone->zbz_write_pointer + lba_count >
                    zone->zbz_start + zone->zbz_length)
                        goto out_unlock;
        } else {
                if (start_lba + lba_count >
                    zone->zbz_start + zone->zbz_length)
                        goto out_unlock;
        }
	pthread_mutex_unlock(&fdev->mutex);

        /* XXX: check for overflows */
        count = (size_t)lba_count * dev->zbd_info.zbd_logical_block_size;
        offset = start_lba * dev->zbd_info.zbd_logical_block_size;

        ret = pwrite(dev->zbd_fd, buf, count, offset);
        if (ret < 0)
                return -errno;

        ret /= dev->zbd_info.zbd_logical_block_size;

	pthread_mutex_lock(&fdev->mutex);
        if (zone->zbz_type != ZBC_ZT_CONVENTIONAL) {
                /*
                * XXX: What protects us from a return value that's not LBA aligned?
                * (Except for hoping the OS implementation isn't insane..)
                */
                zone->zbz_write_pointer += ret;
                if (zone->zbz_write_pointer == zone->zbz_start + zone->zbz_length)
                        zone->zbz_condition = ZBC_ZC_FULL;
                else
                        zone->zbz_condition = ZBC_ZC_OPEN;
        }
        memcpy(z, zone, sizeof(*z));
out_unlock:
	pthread_mutex_unlock(&fdev->mutex);
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
        struct zbc_file_device *fdev = to_file_dev(dev);
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

	pthread_mutex_unlock(&fdev->mutex);

        /* Convert device size into # of physical blocks */
        device_size = dev->zbd_info.zbd_logical_blocks;

        if (conv_zone_size + seq_zone_size > device_size) {
                printf("size: %llu + %llu > %llu\n",
                       (unsigned long long) conv_zone_size,
                       (unsigned long long) seq_zone_size,
                       (unsigned long long) device_size);
                error = -EINVAL;
		goto out_unlock;
        }

        fdev->zbd_nr_zones =
                (device_size - conv_zone_size + seq_zone_size - 1) /
                 seq_zone_size;
        if (conv_zone_size)
                fdev->zbd_nr_zones++;

        len = (off_t)fdev->zbd_nr_zones * sizeof(struct zbc_zone);

        if (fdev->zbd_meta_fd < 0) {
                sprintf(meta_path, "/tmp/zbc-%s.meta",
                        basename(fdev->dev.zbd_filename));
                fdev->zbd_meta_fd = open(meta_path, O_RDWR | O_CREAT, 0600);
                if (fdev->zbd_meta_fd < 0) {
                        perror("open");
                        error = -errno;
			goto out_unlock;
                }
        }

        if (ftruncate(fdev->zbd_meta_fd, len) < 0) {
                perror("ftruncate");
                error = -errno;
                goto out_close;
        }

        fdev->zbd_zones = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
                        fdev->zbd_meta_fd, 0);
        if (fdev->zbd_zones == MAP_FAILED) {
                perror("mmap");
                error = -ENOMEM;
                goto out_close;
        }

        if (conv_zone_size) {
                struct zbc_zone *zone = &fdev->zbd_zones[z];

                zone->zbz_type = ZBC_ZT_CONVENTIONAL;
                zone->zbz_condition = ZBC_ZC_NOT_WP;
                zone->zbz_start = start;
                zone->zbz_write_pointer = 0;
                zone->zbz_length = conv_zone_size;
                zone->zbz_need_reset = false;
                zone->zbz_non_seq = false;
                memset(&zone->__pad, 0, sizeof(zone->__pad));

                start += conv_zone_size;
                z++;
        }

        for (; z < fdev->zbd_nr_zones; z++) {
                struct zbc_zone *zone = &fdev->zbd_zones[z];

                zone->zbz_type = ZBC_ZT_SEQUENTIAL_REQ;
                zone->zbz_condition = ZBC_ZC_EMPTY;

                zone->zbz_start = start;
                zone->zbz_write_pointer = zone->zbz_start;

                if (zone->zbz_start + seq_zone_size <= device_size)
                        zone->zbz_length = seq_zone_size;
                else
                        zone->zbz_length = device_size - zone->zbz_start;

                zone->zbz_need_reset = false;
                zone->zbz_non_seq = false;

                memset(&zone->__pad, 0, sizeof(zone->__pad));

                start += zone->zbz_length;
        }

        error = 0;
out_unlock:
	pthread_mutex_unlock(&fdev->mutex);
        return error;
out_close:
        close(fdev->zbd_meta_fd);
	goto out_unlock;
}

static int
zbc_file_set_write_pointer(struct zbc_device *dev, uint64_t start_lba,
                           uint64_t write_pointer)
{
        struct zbc_file_device *fdev = to_file_dev(dev);
        struct zbc_zone *zone;
	int ret;

	pthread_mutex_lock(&fdev->mutex);
        if (!fdev->zbd_zones) {
                ret = -ENXIO;
		goto out_unlock;
	}

        zone = zbf_file_find_zone(fdev, start_lba);
        if (!zone) {
                ret = -EINVAL;
		goto out_unlock;
	}

        /* Do nothing for conventional zones */
        if (zone->zbz_type != ZBC_ZT_CONVENTIONAL) {
                zone->zbz_write_pointer = write_pointer;
                if (zone->zbz_write_pointer == zone->zbz_start + zone->zbz_length)
                        zone->zbz_condition = ZBC_ZC_FULL;
                else
                        zone->zbz_condition = ZBC_ZC_OPEN;
        }

        ret = 0;
out_unlock:
	pthread_mutex_unlock(&fdev->mutex);
        return ret;
}

struct zbc_ops zbc_file_ops = {
        .zbd_open                       = zbc_file_open,
        .zbd_close                      = zbc_file_close,
        .zbd_pread                      = zbc_file_pread,
        .zbd_pwrite                     = zbc_file_pwrite,
        .zbd_flush                      = zbc_file_flush,
        .zbd_report_zones               = zbc_file_report_zones,
        .zbd_reset_wp                   = zbc_file_reset_wp,
        .zbd_set_zones                  = zbc_file_set_zones,
        .zbd_set_wp                     = zbc_file_set_write_pointer,
};
