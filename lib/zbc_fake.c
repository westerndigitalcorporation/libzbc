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

/***** Macro and types definitions *****/

/**
 * Logical and physical sector size for emulation on top of a regular file.
 * For emulation on top of a raw disk, the disk logical and physical
 * sector sizes are used.
 */
#define ZBC_FAKE_FILE_SECTOR_SIZE       512

/**
 * Maximum number of open zones (implicit + explicit).
 */
#define ZBC_FAKE_MAX_OPEN_NR_ZONES      32

/*
 * Meta-data directory.
 */
#define ZBC_FAKE_META_DIR       	"/var/local"

/**
 * Metadata header.
 */
typedef struct zbc_fake_meta {

    /**
     * Capacity in B.
     */
    uint64_t            zbd_capacity;

    /**
     * Total number of zones.
     */
    uint32_t            zbd_nr_zones;

    /**
     * Number of conventional zones.
     */
    uint32_t            zbd_nr_conv_zones;

    /**
     * Number of sequential zones.
     */
    uint32_t            zbd_nr_seq_zones;

    /**
     * Number of explicitly open zones.
     */
    uint32_t            zbd_nr_exp_open_zones;

    /**
     * Number of implicitely open zones.
     */
    uint32_t            zbd_nr_imp_open_zones;

    /**
     * Process shared mutex.
     */
    pthread_mutex_t     zbd_mutex;

} zbc_fake_meta_t;

/**
 * Fake device descriptor data.
 */
typedef struct zbc_fake_device {

    struct zbc_device   dev;

    int                 zbd_meta_fd;
    size_t              zbd_meta_size;
    zbc_fake_meta_t     *zbd_meta;

    pthread_mutexattr_t zbd_mutex_attr;

    uint32_t            zbd_nr_zones;
    struct zbc_zone     *zbd_zones;

} zbc_fake_device_t;

/***** Definition of private functions *****/

/**
 * Build meta-data file path for a device.
 */
static inline void
zbc_fake_dev_meta_path(zbc_fake_device_t *fdev,
    		       char *buf)
{
    sprintf(buf, "%s/zbc-%s.meta",
            ZBC_FAKE_META_DIR,
            basename(fdev->dev.zbd_filename));

    return;
}

/**
 * Convert device address to fake device address.
 */
static inline zbc_fake_device_t *
zbc_fake_to_file_dev(struct zbc_device *dev)
{
    return container_of(dev, struct zbc_fake_device, dev);
}

/**
 * Find a zone using its start LBA.
 */
static struct zbc_zone *
zbc_fake_find_zone(zbc_fake_device_t *fdev,
                   uint64_t zone_start_lba)
{
    unsigned int i;

    if ( fdev->zbd_zones ) {
        for(i = 0; i < fdev->zbd_nr_zones; i++) {
            if ( fdev->zbd_zones[i].zbz_start == zone_start_lba ) {
                return &fdev->zbd_zones[i];
            }
        }
    }

    return NULL;

}

/**
 * Lock a device metadata.
 */
static void
zbc_fake_lock(zbc_fake_device_t *fdev)
{

    pthread_mutex_lock(&fdev->zbd_meta->zbd_mutex);

    return;

}

/**
 * Unlock a device metadata.
 */
static void
zbc_fake_unlock(zbc_fake_device_t *fdev)
{

    pthread_mutex_unlock(&fdev->zbd_meta->zbd_mutex);

    return;

}

/**
 * Close metadata file of a fake device.
 */
static void
zbc_fake_close_metadata(zbc_fake_device_t *fdev)
{

    if ( fdev->zbd_meta_fd > 0 ) {

        if ( fdev->zbd_meta ) {
            msync(fdev->zbd_meta, fdev->zbd_meta_size, MS_SYNC);
            munmap(fdev->zbd_meta, fdev->zbd_meta_size);
            fdev->zbd_meta = NULL;
            fdev->zbd_meta_size = 0;
        }

        close(fdev->zbd_meta_fd);
        fdev->zbd_meta_fd = -1;

        pthread_mutexattr_destroy(&fdev->zbd_mutex_attr);

    }

    return;

}

/**
 * Open metadata file of a fake device.
 */
static int
zbc_fake_open_metadata(zbc_fake_device_t *fdev)
{
    char meta_path[512];
    struct stat st;
    int ret;

    zbc_fake_dev_meta_path(fdev, meta_path);

    zbc_debug("%s: using meta file %s\n",
              fdev->dev.zbd_filename,
              meta_path);

    pthread_mutexattr_init(&fdev->zbd_mutex_attr);
    pthread_mutexattr_setpshared(&fdev->zbd_mutex_attr, PTHREAD_PROCESS_SHARED);

    fdev->zbd_meta_fd = open(meta_path, O_RDWR);
    if ( fdev->zbd_meta_fd < 0 ) {
        /* Metadata does not exist yet, we'll have to wait for a set_zones call */
        if ( errno == ENOENT ) {
            return 0;
        }
        ret = -errno;
        zbc_error("%s: open metadata file %s failed %d (%s)\n",
                  fdev->dev.zbd_filename,
                  meta_path,
                  errno,
                  strerror(errno));
        goto out;
    }

    if ( fstat(fdev->zbd_meta_fd, &st) < 0 ) {
        zbc_error("%s: fstat metadata file %s failed %d (%s)\n",
                  fdev->dev.zbd_filename,
                  meta_path,
                  errno,
                  strerror(errno));
        ret = -errno;
        goto out;
    }

    /* mmap metadata file */
    fdev->zbd_meta_size = st.st_size;
    fdev->zbd_meta = mmap(NULL,
                          fdev->zbd_meta_size,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fdev->zbd_meta_fd,
                          0);
    if ( fdev->zbd_meta == MAP_FAILED ) {
        fdev->zbd_meta = NULL;
        zbc_error("%s: mmap metadata file %s failed\n",
                  fdev->dev.zbd_filename,
                  meta_path);
        ret = -ENOMEM;
        goto out;
    }

    /* Check */
    if ( (fdev->zbd_meta->zbd_capacity != (fdev->dev.zbd_info.zbd_logical_block_size * fdev->dev.zbd_info.zbd_logical_blocks))
         || (! fdev->zbd_meta->zbd_nr_zones) ) {
	/* Do not report an error here to allow the execution of zbc_set_zones */
        zbc_debug("%s: invalid metadata file %s\n",
                  fdev->dev.zbd_filename,
                  meta_path);
	zbc_fake_close_metadata(fdev);
        ret = 0;
        goto out;
    }

    zbc_debug("%s: %llu sectors of %zuB, %u zones\n",
              fdev->dev.zbd_filename,
	      (unsigned long long)fdev->dev.zbd_info.zbd_logical_blocks,
	      (size_t)fdev->dev.zbd_info.zbd_logical_block_size,
              fdev->zbd_meta->zbd_nr_zones);

    fdev->zbd_nr_zones = fdev->zbd_meta->zbd_nr_zones;
    fdev->zbd_zones = (struct zbc_zone *) (fdev->zbd_meta + 1);

    ret = 0;

out:

    if ( ret != 0 ) {
        zbc_fake_close_metadata(fdev);
    }

    return ret;
}

/**
 * Set a device info.
 */
static int
zbc_fake_set_info(struct zbc_device *dev)
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

    if ( S_ISBLK(st.st_mode) ) {

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

    } else if ( S_ISREG(st.st_mode) ) {

        /* Default value for files */
        dev->zbd_info.zbd_logical_block_size = ZBC_FAKE_FILE_SECTOR_SIZE;
        dev->zbd_info.zbd_logical_blocks = st.st_size / ZBC_FAKE_FILE_SECTOR_SIZE;
        dev->zbd_info.zbd_physical_block_size = dev->zbd_info.zbd_logical_block_size;
        dev->zbd_info.zbd_physical_blocks = dev->zbd_info.zbd_logical_blocks;

    } else {

        return -ENXIO;

    }

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
    dev->zbd_info.zbd_type = ZBC_DT_FAKE;
    dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
    strncpy(dev->zbd_info.zbd_vendor_id, "FAKE HGST HM libzbc", ZBC_DEVICE_INFO_LENGTH - 1);

    dev->zbd_info.zbd_opt_nr_open_seq_pref = 0;
    dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref = 0;
    dev->zbd_info.zbd_max_nr_open_seq_req = ZBC_FAKE_MAX_OPEN_NR_ZONES;

    return 0;

}

/**
 * Open an emulation device or file.
 */
static int
zbc_fake_open(const char *filename,
              int flags,
              struct zbc_device **pdev)
{
    zbc_fake_device_t *fdev;
    int fd, ret;

    zbc_debug("%s: ########## Trying FAKE driver ##########\n",
	      filename);

    /* Open emulation device/file */
    fd = open(filename, zbc_open_flags(flags));
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
    fdev = calloc(1, sizeof(*fdev));
    if ( ! fdev ) {
        goto out;
    }

    fdev->dev.zbd_fd = fd;
    fdev->zbd_meta_fd = -1;
    fdev->dev.zbd_filename = strdup(filename);
    if ( ! fdev->dev.zbd_filename ) {
        goto out_free_dev;
    }

    /* Set the fake device information */
    ret = zbc_fake_set_info(&fdev->dev);
    if ( ret != 0 ) {
        goto out_free_filename;
    }

    /* Open metadata */
    ret = zbc_fake_open_metadata(fdev);
    if ( ret ) {
        goto out_free_filename;
    }

    *pdev = &fdev->dev;

    zbc_debug("%s: ########## FAKE driver succeeded ##########\n",
	      filename);

    return 0;

out_free_filename:

    free(fdev->dev.zbd_filename);

out_free_dev:

    free(fdev);

out:

    if ( fd >= 0 ) {
	close(fd);
    }

    zbc_debug("%s: ########## FAKE driver failed %d ##########\n",
	      filename,
	      ret);

    return ret;

}

/**
 * close a device.
 */
static int
zbc_fake_close(zbc_device_t *dev)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    int ret = 0;

    /* Close metadata */
    zbc_fake_close_metadata(fdev);

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
 * Test if a zone must be reported.
 */
static bool
zbc_fake_must_report_zone(struct zbc_zone *zone,
                          uint64_t start_lba,
                          enum zbc_reporting_options ro)
{
    enum zbc_reporting_options options = ro & (~ZBC_RO_PARTIAL);

    if ( (zbc_zone_length(zone) == 0)
         || (zbc_zone_start_lba(zone) < start_lba) ) {
        return false;
    }

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
 * Get device zone information.
 */
static int
zbc_fake_report_zones(struct zbc_device *dev,
                      uint64_t start_lba,
                      enum zbc_reporting_options ro,
		      uint64_t *max_lba,
                      struct zbc_zone *zones,
                      unsigned int *nr_zones)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    unsigned int max_nr_zones = *nr_zones;
    enum zbc_reporting_options options = ro & (~ZBC_RO_PARTIAL);
    unsigned int in, out = 0;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    /* Check reporting option */
    if ( (options != ZBC_RO_ALL)
	 && (options != ZBC_RO_EMPTY)
         && (options != ZBC_RO_IMP_OPEN)
	 && (options != ZBC_RO_EXP_OPEN)
         && (options != ZBC_RO_CLOSED)
	 && (options != ZBC_RO_FULL)
         && (options != ZBC_RO_RDONLY)
	 && (options != ZBC_RO_OFFLINE)
         && (options != ZBC_RO_RESET)
	 && (options != ZBC_RO_NON_SEQ)
	 && (options != ZBC_RO_NOT_WP) ) {
        dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
        dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
        return -EIO;
    }

    /* Check start_lba */
    if ( start_lba >= dev->zbd_info.zbd_logical_blocks) {
        dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
        dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        return -EIO;
    }

    zbc_fake_lock(fdev);

    if ( max_lba ) {
	*max_lba = dev->zbd_info.zbd_logical_blocks - 1;
    }

    if ( ! zones ) {

        /* Only get the number of matching zones */
        for(in = 0; in < fdev->zbd_nr_zones; in++) {
            if ( zbc_fake_must_report_zone(&fdev->zbd_zones[in], start_lba, options) ) {
                out++;
            }
        }

    } else {

        /* Get matching zones */
        for(in = 0; in < fdev->zbd_nr_zones; in++) {
            if ( zbc_fake_must_report_zone(&fdev->zbd_zones[in], start_lba, options) ) {
		 if ( out < max_nr_zones ) {
		     memcpy(&zones[out], &fdev->zbd_zones[in], sizeof(struct zbc_zone));
		 }
		 out++;
            }
	    if ( (out >= max_nr_zones) && (ro & ZBC_RO_PARTIAL) ) {
		break;
	    }
        }

    }

    if ( out > max_nr_zones ) {
	out = max_nr_zones;
    }

    *nr_zones = out;

    zbc_fake_unlock(fdev);

    return 0;

}

/**
 * Close a zone.
 */
static void
zbc_zone_do_close(zbc_fake_device_t *fdev,
                  struct zbc_zone *zone)
{

    if ( zbc_zone_is_open(zone) ) {

        if ( zbc_zone_imp_open(zone) ) {
            fdev->zbd_meta->zbd_nr_imp_open_zones--;;
        } else if ( zbc_zone_exp_open(zone) ) {
            fdev->zbd_meta->zbd_nr_exp_open_zones--;
        }

        if ( zbc_zone_wp_lba(zone) == zbc_zone_start_lba(zone) ) {
            zone->zbz_condition = ZBC_ZC_EMPTY;
        } else {
            zone->zbz_condition = ZBC_ZC_CLOSED;
        }

    }

    return;

}

/**
 * Open zone(s).
 */
static int
zbc_fake_open_zone(zbc_device_t *dev,
                   uint64_t start_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    unsigned int i;
    int ret = 0;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    if ( start_lba == (uint64_t)-1 ) {

        unsigned int need_open = 0;

        /* Check if all closed zones can be open */
        for(i = 0; i < fdev->zbd_nr_zones; i++) {
            if ( zbc_zone_closed(&fdev->zbd_zones[i]) ) {
                need_open++;
            }
        }
        if ( (fdev->zbd_meta->zbd_nr_exp_open_zones + need_open) > fdev->dev.zbd_info.zbd_max_nr_open_seq_req ) {
            dev->zbd_errno.sk = ZBC_E_ABORTED_COMMAND;
            dev->zbd_errno.asc_ascq = ZBC_E_INSUFFICIENT_ZONE_RESOURCES;
            ret = -EIO;
            goto out;
        }

        /* Open all closed zones */
        for(i = 0; i < fdev->zbd_nr_zones; i++) {
            if ( zbc_zone_closed(&fdev->zbd_zones[i]) ) {
                fdev->zbd_zones[i].zbz_condition = ZBC_ZC_EXP_OPEN;
            }
        }
        fdev->zbd_meta->zbd_nr_exp_open_zones += need_open;

    } else {

        struct zbc_zone *zone;

        /* Check start_lba */
        if ( start_lba > dev->zbd_info.zbd_logical_blocks - 1) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            return -EIO;
        }

        /* Check target zone */
        zone = zbc_fake_find_zone(fdev, start_lba);
        if ( ! zone ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_conventional(zone) ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_full(zone) ) {
            /* Full zone open: do nothing (condition remains full) */
            goto out;
        }

        if ( zbc_zone_exp_open(zone) ) {
            /* Already open: nothing to do */
            goto out;
        }

        if ( ! (zbc_zone_closed(zone)
                || zbc_zone_imp_open(zone)
                || zbc_zone_empty(zone)) ) {
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_imp_open(zone) ) {
            zbc_zone_do_close(fdev, zone);
        }

        /* Check limit */
        if ( (fdev->zbd_meta->zbd_nr_exp_open_zones + fdev->zbd_meta->zbd_nr_imp_open_zones + 1)
             > fdev->dev.zbd_info.zbd_max_nr_open_seq_req ) {

            if ( ! fdev->zbd_meta->zbd_nr_imp_open_zones ) {
                dev->zbd_errno.sk = ZBC_E_ABORTED_COMMAND;
                dev->zbd_errno.asc_ascq = ZBC_E_INSUFFICIENT_ZONE_RESOURCES;
                ret = -EIO;
                goto out;
            }

            /* Close an implicitely open zone */
            for(i = 0; i < fdev->zbd_nr_zones; i++) {
                if ( zbc_zone_imp_open(&fdev->zbd_zones[i]) ) {
                    zbc_zone_do_close(fdev, &fdev->zbd_zones[i]);
                    break;
                }
            }

        }

        /* Open the specified zone */
        zone->zbz_condition = ZBC_ZC_EXP_OPEN;
        fdev->zbd_meta->zbd_nr_exp_open_zones++;

    }

out:

    zbc_fake_unlock(fdev);

    return ret;

}

/**
 * Test if a zone can be closed.
 */
static bool
zbc_zone_close_allowed(struct zbc_zone *zone)
{
    return zbc_zone_sequential(zone)
        && (zbc_zone_empty(zone)
            || zbc_zone_full(zone)
            || zbc_zone_imp_open(zone)
            || zbc_zone_exp_open(zone));
}

/**
 * Close zone(s).
 */
static int
zbc_fake_close_zone(zbc_device_t *dev,
                    uint64_t start_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    int ret = 0;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    if ( start_lba == (uint64_t)-1 ) {

        unsigned int i;

        /* Close all open zones */
        for(i = 0; i < fdev->zbd_nr_zones; i++) {
            if ( zbc_zone_close_allowed(&fdev->zbd_zones[i]) ) {
                zbc_zone_do_close(fdev, &fdev->zbd_zones[i]);
            }
        }

    } else {

        struct zbc_zone *zone;

        /* Check start_lba */
        if ( start_lba > dev->zbd_info.zbd_logical_blocks - 1) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            return -EIO;
        }

        /* Close the specified zone */
        zone = zbc_fake_find_zone(fdev, start_lba);
        if ( ! zone ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_conventional(zone) ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_close_allowed(zone) ) {
            zbc_zone_do_close(fdev, zone);
        } else if ( ! zbc_zone_closed(zone) ) {
            ret = -EIO;
        }

    }

out:

    zbc_fake_unlock(fdev);

    return ret;

}

/**
 * Test if a zone can be finished.
 */
static bool
zbc_zone_finish_allowed(struct zbc_zone *zone)
{
    return zbc_zone_sequential(zone)
        && (zbc_zone_imp_open(zone)
            || zbc_zone_exp_open(zone)
            || zbc_zone_closed(zone));
}

/**
 * Finish a zone.
 */
static void
zbc_zone_do_finish(zbc_fake_device_t *fdev,
                   struct zbc_zone *zone)
{

    if ( zbc_zone_is_open(zone) ) {
        zbc_zone_do_close(fdev, zone);
    }

    zone->zbz_write_pointer = (uint64_t)-1;
    zone->zbz_condition = ZBC_ZC_FULL;

    return;

}

/**
 * Finish zone(s).
 */
static int
zbc_fake_finish_zone(zbc_device_t *dev,
                     uint64_t start_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    int ret = 0;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    if ( start_lba == (uint64_t)-1 ) {

        unsigned int i;

        /* Finish all open and closed zones */
        for(i = 0; i < fdev->zbd_nr_zones; i++) {
            if ( zbc_zone_finish_allowed(&fdev->zbd_zones[i]) ) {
                zbc_zone_do_finish(fdev, &fdev->zbd_zones[i]);
            }
        }

    } else {

        struct zbc_zone *zone;

        /* Check start_lba */
        if ( start_lba > dev->zbd_info.zbd_logical_blocks - 1) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            return -EIO;
        }

        /* Finish the specified zone */
        zone = zbc_fake_find_zone(fdev, start_lba);
        if ( ! zone ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_conventional(zone) ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_finish_allowed(zone) || zbc_zone_empty(zone) ) {
            zbc_zone_do_finish(fdev, zone);
        } else if ( ! zbc_zone_full(zone) ) {
            ret = -EIO;
        }

    }

out:

    zbc_fake_unlock(fdev);

    return ret;

}

/**
 * Test if a zone write pointer can be reset.
 */
static bool
zbc_zone_reset_allowed(struct zbc_zone *zone)
{
    return zbc_zone_sequential(zone)
        && (zbc_zone_imp_open(zone)
            || zbc_zone_exp_open(zone)
            || zbc_zone_closed(zone)
            || zbc_zone_empty(zone)
            || zbc_zone_full(zone));
}

/**
 * Reset a zone write pointer.
 */
static void
zbc_zone_do_reset(zbc_fake_device_t *fdev,
                  struct zbc_zone *zone)
{

    if ( ! zbc_zone_empty(zone) ) {

        if ( zbc_zone_is_open(zone) ) {
            zbc_zone_do_close(fdev, zone);
        }

        zone->zbz_write_pointer = zbc_zone_start_lba(zone);
        zone->zbz_condition = ZBC_ZC_EMPTY;

    }

    return;

}

/**
 * Reset zone(s) write pointer.
 */
static int
zbc_fake_reset_wp(struct zbc_device *dev,
                  uint64_t start_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    int ret = 0;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    if ( start_lba == (uint64_t)-1 ) {

        unsigned int i;

        /* Reset all open, closed and full zones */
        for(i = 0; i < fdev->zbd_nr_zones; i++) {
            if ( zbc_zone_reset_allowed(&fdev->zbd_zones[i]) ) {
                zbc_zone_do_reset(fdev, &fdev->zbd_zones[i]);
            }
        }

    } else {

        struct zbc_zone *zone;

        /* Check start_lba */
        if ( start_lba > dev->zbd_info.zbd_logical_blocks - 1) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            return -EIO;
        }

        /* Reset the specified zone */
        zone = zbc_fake_find_zone(fdev, start_lba);
        if ( ! zone ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_conventional(zone) ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_INVALID_FIELD_IN_CDB;
            ret = -EIO;
            goto out;
        }

        if ( zbc_zone_reset_allowed(zone) ) {
            zbc_zone_do_reset(fdev, zone);
        } else if ( ! zbc_zone_empty(zone) ) {
            ret = -EIO;
        }

    }

out:

    zbc_fake_unlock(fdev);

    return ret;

}

/**
 * Read from the emulated device/file.
 */
static int32_t
zbc_fake_pread(struct zbc_device *dev,
               struct zbc_zone *z,
               void *buf,
               uint32_t lba_count,
               uint64_t start_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    struct zbc_zone *zone, *next_zone;
    uint64_t lba;
    ssize_t ret = -EIO;
    off_t offset;
    size_t count;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    /* Find the target zone */
    zone = zbc_fake_find_zone(fdev, zbc_zone_start_lba(z));
    if ( ! zone ) {
        goto out;
    }

    if ( start_lba > zbc_zone_length(zone) ) {
        goto out;
    }

    lba = zbc_zone_next_lba(zone);
    next_zone = zbc_fake_find_zone(fdev, lba);
    start_lba += zbc_zone_start_lba(zone);

    /* Note: unrestricted read will be added to the standard */
    /* and supported by a drive if the URSWRZ bit is set in  */
    /* VPD page. So this test will need to change.           */
    if ( zone->zbz_type == ZBC_ZT_SEQUENTIAL_REQ ) {

        /* Cannot read unwritten data */
        if ( (start_lba + lba_count) > lba ) {
            if ( next_zone ) {
                dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
                dev->zbd_errno.asc_ascq = ZBC_E_READ_BOUNDARY_VIOLATION;
            } else {
                dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
                dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            }
            goto out;
        }

        if ( (start_lba + lba_count) > zbc_zone_wp_lba(zone) ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_ATTEMPT_TO_READ_INVALID_DATA;
            goto out;
        }

    } else {

        /* Reads spanning other types of zones are OK. */

        if ( (start_lba + lba_count) > lba ) {

            uint64_t count = start_lba + lba_count - lba;

            while( count && next_zone ) {
                if ( zbc_zone_sequential_req(next_zone) ) {
                    dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
                    dev->zbd_errno.asc_ascq = ZBC_E_ATTEMPT_TO_READ_INVALID_DATA;
                    goto out;
                }
                if ( count > zbc_zone_length(next_zone) ) {
                    count -= zbc_zone_length(next_zone);
                }
                lba += zbc_zone_length(next_zone);
            }

        }

    }

    /* XXX: check for overflows */
    count = lba_count * dev->zbd_info.zbd_logical_block_size;
    offset = start_lba * dev->zbd_info.zbd_logical_block_size;

    ret = pread(dev->zbd_fd, buf, count, offset);
    if ( ret < 0 ) {
        ret = -errno;
    } else {
        ret /= dev->zbd_info.zbd_logical_block_size;
    }

out:

    zbc_fake_unlock(fdev);

    return ret;

}

/**
 * Write to the emulated device/file.
 */
static int32_t
zbc_fake_pwrite(struct zbc_device *dev,
                struct zbc_zone *z,
                const void *buf,
                uint32_t lba_count,
                uint64_t start_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    struct zbc_zone *zone, *next_zone;
    uint64_t lba;
    off_t offset;
    size_t count;
    ssize_t ret = -EIO;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    /* Find the target zone */
    zone = zbc_fake_find_zone(fdev, zbc_zone_start_lba(z));
    if ( ! zone ) {
        goto out;
    }

    /* Writes cannot span zones */
    if ( start_lba > zbc_zone_length(zone) ) {
        goto out;
    }

    lba = zbc_zone_next_lba(zone);
    next_zone = zbc_fake_find_zone(fdev, lba);
    start_lba += zone->zbz_start;

    if ( (start_lba + lba_count) > lba ) {
        if ( next_zone ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_WRITE_BOUNDARY_VIOLATION;
        } else {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        }
        goto out;
    }

    if ( zbc_zone_sequential_req(zone) ) {

        /* Can only write at the write pointer */
        if ( start_lba != zbc_zone_wp_lba(zone) ) {
            dev->zbd_errno.sk = ZBC_E_ILLEGAL_REQUEST;
            dev->zbd_errno.asc_ascq = ZBC_E_UNALIGNED_WRITE_COMMAND;
            goto out;
        }

        /* Can only write an open zone */
        if ( ! zbc_zone_is_open(zone) ) {

            if ( fdev->zbd_meta->zbd_nr_exp_open_zones >= fdev->dev.zbd_info.zbd_max_nr_open_seq_req ) {
                /* Too many explicit open on-going */
                dev->zbd_errno.sk = ZBC_E_ABORTED_COMMAND;
                dev->zbd_errno.asc_ascq = ZBC_E_INSUFFICIENT_ZONE_RESOURCES;
                ret = -EIO;
                goto out;
            }

            /* Implicitely open the zone */
            if ( fdev->zbd_meta->zbd_nr_imp_open_zones >= fdev->dev.zbd_info.zbd_max_nr_open_seq_req ) {
                unsigned int i;
                for(i = 0; i < fdev->zbd_nr_zones; i++) {
                    if ( zbc_zone_imp_open(&fdev->zbd_zones[i]) ) {
                        zbc_zone_do_close(fdev, &fdev->zbd_zones[i]);
                        break;
                    }
                }
            }

            zone->zbz_condition = ZBC_ZC_IMP_OPEN;
            fdev->zbd_meta->zbd_nr_imp_open_zones++;

        }

    }

    /* XXX: check for overflows */
    count = (size_t)lba_count * dev->zbd_info.zbd_logical_block_size;
    offset = start_lba * dev->zbd_info.zbd_logical_block_size;

    ret = pwrite(dev->zbd_fd, buf, count, offset);
    if ( ret < 0 ) {

        ret = -errno;

    } else {

        ret /= dev->zbd_info.zbd_logical_block_size;

        if ( zbc_zone_sequential_req(zone) ) {

            /*
             * XXX: What protects us from a return value that's not LBA aligned?
             * (Except for hoping the OS implementation isn't insane..)
             */
            if ( (zbc_zone_wp_lba(zone) + lba_count) >= zbc_zone_next_lba(zone) ) {
                if ( zbc_zone_imp_open(zone) ) {
                    fdev->zbd_meta->zbd_nr_imp_open_zones--;
                } else {
                    fdev->zbd_meta->zbd_nr_exp_open_zones--;
                }
            }

            /* Advance write pointer */
            zbc_zone_wp_lba_inc(zone, lba_count);

        }

    }

out:

    zbc_fake_unlock(fdev);

    return ret;

}

/**
 * Flush the emulated device data and metadata.
 */
static int
zbc_fake_flush(struct zbc_device *dev,
               uint64_t lba_offset,
               uint32_t lba_count,
               int immediate)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    int ret;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    ret = msync(fdev->zbd_meta, fdev->zbd_meta_size, MS_SYNC);
    if ( ret == 0 ) {
        ret = fsync(dev->zbd_fd);
    }

    return ret;

}

/**
 * Initialize an emulated device metadata.
 */
static int
zbc_fake_set_zones(struct zbc_device *dev,
                   uint64_t conv_sz,
                   uint64_t zone_sz)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    uint64_t lba = 0, device_size = dev->zbd_info.zbd_logical_blocks;
    zbc_fake_meta_t fmeta;
    char meta_path[512];
    struct zbc_zone *zone;
    unsigned int z = 0;
    int ret;

    /* Initialize metadata */
    if ( fdev->zbd_meta ) {
        zbc_fake_close_metadata(fdev);
    }

    memset(&fmeta, 0, sizeof(zbc_fake_meta_t));

    pthread_mutexattr_init(&fdev->zbd_mutex_attr);
    pthread_mutexattr_setpshared(&fdev->zbd_mutex_attr, PTHREAD_PROCESS_SHARED);

    /* Calculate zone configuration */
    if ( (conv_sz + zone_sz) > device_size ) {
        zbc_error("%s: invalid zone sizes (too large)\n",
                  fdev->dev.zbd_filename);
        return -EINVAL;
    }

    fmeta.zbd_nr_conv_zones = conv_sz / zone_sz;
    if ( conv_sz && (! fmeta.zbd_nr_conv_zones) ) {
        fmeta.zbd_nr_conv_zones = 1;
    }

    fmeta.zbd_nr_seq_zones = (device_size - (fmeta.zbd_nr_conv_zones * zone_sz)) / zone_sz;
    if ( ! fmeta.zbd_nr_seq_zones ) {
        zbc_error("%s: invalid zone sizes (too large)\n",
                  fdev->dev.zbd_filename);
        return -EINVAL;
    }

    fmeta.zbd_nr_zones = fmeta.zbd_nr_conv_zones + fmeta.zbd_nr_seq_zones;
    fdev->zbd_nr_zones = fmeta.zbd_nr_zones;

    dev->zbd_info.zbd_logical_blocks = fdev->zbd_nr_zones * zone_sz;
    dev->zbd_info.zbd_physical_blocks = dev->zbd_info.zbd_logical_blocks /
                                        (dev->zbd_info.zbd_physical_block_size / dev->zbd_info.zbd_logical_block_size);
    fmeta.zbd_capacity = dev->zbd_info.zbd_logical_blocks * dev->zbd_info.zbd_logical_block_size;

    /* Open metadata file */
    zbc_fake_dev_meta_path(fdev, meta_path);
    fdev->zbd_meta_fd = open(meta_path, O_RDWR | O_CREAT, 0600);
    if ( fdev->zbd_meta_fd < 0 ) {
        ret = -errno;
        zbc_error("%s: open metadata file %s failed %d (%s)\n",
                  fdev->dev.zbd_filename,
                  meta_path,
                  errno,
                  strerror(errno));
        return ret;
    }

    /* Truncate metadata file */
    fdev->zbd_meta_size = sizeof(zbc_fake_meta_t) + (fdev->zbd_nr_zones * sizeof(struct zbc_zone));
    if ( ftruncate(fdev->zbd_meta_fd, fdev->zbd_meta_size) < 0) {
        ret = -errno;
        zbc_error("%s: truncate metadata file %s to %zu B failed %d (%s)\n",
                  fdev->dev.zbd_filename,
                  meta_path,
                  fdev->zbd_meta_size,
                  errno,
                  strerror(errno));
        goto out;
    }

    /* mmap metadata file */
    fdev->zbd_meta = mmap(NULL, fdev->zbd_meta_size, PROT_READ | PROT_WRITE, MAP_SHARED, fdev->zbd_meta_fd, 0);
    if ( fdev->zbd_meta == MAP_FAILED ) {
        fdev->zbd_meta = NULL;
        zbc_error("%s: mmap metadata file %s failed\n",
                  fdev->dev.zbd_filename,
                  meta_path);
        ret = -ENOMEM;
        goto out;
    }

    fdev->zbd_zones = (struct zbc_zone *) (fdev->zbd_meta + 1);

    /* Setup metadata header */
    memcpy(fdev->zbd_meta, &fmeta, sizeof(zbc_fake_meta_t));
    ret = pthread_mutex_init(&fdev->zbd_meta->zbd_mutex, &fdev->zbd_mutex_attr);
    if ( ret != 0 ) {
        zbc_error("%s: Initialize metadata mutex failed %d (%s)\n",
                  fdev->dev.zbd_filename,
                  ret,
                  strerror(ret));
        goto out;
    }

    /* Setup conventional zones descriptors */
    for(z = 0; z < fmeta.zbd_nr_conv_zones; z++) {

        zone = &fdev->zbd_zones[z];

        zone->zbz_type = ZBC_ZT_CONVENTIONAL;
        zone->zbz_condition = ZBC_ZC_NOT_WP;
        zone->zbz_start = lba;
        zone->zbz_write_pointer = (uint64_t)-1;
        zone->zbz_length = zone_sz;

        memset(&zone->__pad, 0, sizeof(zone->__pad));

        lba += zone_sz;

    }

    /* Setup sequential zones descriptors */
    for (; z < fdev->zbd_nr_zones; z++) {

        zone = &fdev->zbd_zones[z];

        zone->zbz_type = ZBC_ZT_SEQUENTIAL_REQ;
        zone->zbz_condition = ZBC_ZC_EMPTY;

        zone->zbz_start = lba;
        zone->zbz_write_pointer = zone->zbz_start;
        zone->zbz_length = zone_sz;

        memset(&zone->__pad, 0, sizeof(zone->__pad));

        lba += zone_sz;

    }

    ret = 0;

out:

    if ( ret != 0 ) {
        zbc_fake_close_metadata(fdev);
    }

    return ret;

}

/**
 * Change the value of a zone write pointer.
 */
static int
zbc_fake_set_write_pointer(struct zbc_device *dev,
                           uint64_t start_lba,
                           uint64_t wp_lba)
{
    zbc_fake_device_t *fdev = zbc_fake_to_file_dev(dev);
    struct zbc_zone *zone;
    int ret = -EIO;

    if ( ! fdev->zbd_meta ) {
        return -ENXIO;
    }

    zbc_fake_lock(fdev);

    zone = zbc_fake_find_zone(fdev, start_lba);
    if ( zone ) {

        /* Do nothing for conventional zones */
        if ( zbc_zone_sequential_req(zone) ) {

            if ( zbc_zone_is_open(zone) ) {
                zbc_zone_do_close(fdev, zone);
            }

            zone->zbz_write_pointer = wp_lba;
            if ( zbc_zone_wp_lba(zone) == zbc_zone_start_lba(zone) ) {
                zone->zbz_condition = ZBC_ZC_EMPTY;
            } else if ( zbc_zone_wp_within_zone(zone) ) {
                zone->zbz_condition = ZBC_ZC_CLOSED;
            } else {
                zone->zbz_condition = ZBC_ZC_FULL;
                zone->zbz_write_pointer = (uint64_t)-1;
            }

        }

        ret = 0;

    }

    zbc_fake_unlock(fdev);

    return ret;

}

struct zbc_ops zbc_fake_ops = {
    .zbd_open         = zbc_fake_open,
    .zbd_close        = zbc_fake_close,
    .zbd_pread        = zbc_fake_pread,
    .zbd_pwrite       = zbc_fake_pwrite,
    .zbd_flush        = zbc_fake_flush,
    .zbd_report_zones = zbc_fake_report_zones,
    .zbd_open_zone    = zbc_fake_open_zone,
    .zbd_close_zone   = zbc_fake_close_zone,
    .zbd_finish_zone  = zbc_fake_finish_zone,
    .zbd_reset_wp     = zbc_fake_reset_wp,
    .zbd_set_zones    = zbc_fake_set_zones,
    .zbd_set_wp       = zbc_fake_set_write_pointer,
};
