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
 * Authors: Damien Le Moal (damien.lemoal@hgst.com)
 *          Christoph Hellwig (hch@infradead.org)
 */

/***** Including files *****/

#include "zbc.h"

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/fs.h>

/***** Definition of private data *****/

/**
 * Log level.
 */
int zbc_log_level = ZBC_LOG_ERROR;

/**
 * Backend drivers.
 */
static struct zbc_ops *zbc_ops[] = {
    &zbc_block_ops,
    &zbc_ata_ops,
    &zbc_scsi_ops,
    &zbc_fake_ops,
    NULL
};

/**
 * Sense key, ASC/ASCQ
 */
static struct zbc_sg_sk_s
{

    enum zbc_sk         sk;
    const char          *sk_name;

} zbc_sg_sk_list[] = {

    /* ILLEGAL_REQUEST */
    {
        ZBC_E_ILLEGAL_REQUEST,
        "Illegal-request"
    },

    /* DATA_PROTECT */
    {
        ZBC_E_DATA_PROTECT,
        "Data-protect",
    },

    /* ABORTED_COMMAND */
    {
        ZBC_E_ABORTED_COMMAND,
        "Aborted-command",
    },

    /* Unknown */
    {
	0,
	NULL,
    }

};

static struct zbc_sg_asc_ascq_s
{

    enum zbc_asc_ascq   asc_ascq;
    const char          *ascq_name;

} zbc_sg_asc_ascq_list[] = {

    /* ZBC_E_INVALID_FIELD_IN_CDB */
    {
        ZBC_E_INVALID_FIELD_IN_CDB,
        "Invalid-field-in-cdb"
    },

    /* ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE */
    {
        ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE,
        "Logical-block-address-out-of-range"
    },

    /* ZBC_E_UNALIGNED_WRITE_COMMAND */
    {
        ZBC_E_UNALIGNED_WRITE_COMMAND,
        "Unaligned-write-command"
    },

    /* ZBC_E_WRITE_BOUNDARY_VIOLATION */
    {
        ZBC_E_WRITE_BOUNDARY_VIOLATION,
        "Write-boundary-violation"
    },

    /* ZBC_E_ATTEMPT_TO_READ_INVALID_DATA */
    {
        ZBC_E_ATTEMPT_TO_READ_INVALID_DATA,
        "Attempt-to-read-invalid-data"
    },

    /* ZBC_E_READ_BOUNDARY_VIOLATION */
    {
        ZBC_E_READ_BOUNDARY_VIOLATION,
        "Read-boundary-violation"
    },

    /* ZBC_E_ZONE_IS_READ_ONLY */
    {
        ZBC_E_ZONE_IS_READ_ONLY,
        "Zone-is-read-only"
    },

    /* ZBC_E_INSUFFICIENT_ZONE_RESOURCES */
    {
        ZBC_E_INSUFFICIENT_ZONE_RESOURCES,
        "Insufficient-zone-resources"
    },

    /* Unknown */
    {
	0,
	NULL,
    }

};

/***** Declaration of private funtions *****/

static inline int
zbc_do_report_zones(zbc_device_t *dev,
                    uint64_t start_lba,
                    enum zbc_reporting_options ro,
		    uint64_t *max_lba,
                    zbc_zone_t *zones,
                    unsigned int *nr_zones)
{

    /* Nothing much to do here: just call the device command operation */
    return( (dev->zbd_ops->zbd_report_zones)(dev, start_lba, ro, max_lba, zones, nr_zones) );

}

/***** Definition of public functions *****/

/**
 * Set library log level.
 */
void
zbc_set_log_level(char *log_level)
{

    if ( log_level ) {
        if ( strcmp(log_level, "none") == 0 ) {
            zbc_log_level = ZBC_LOG_NONE;
        } else if ( strcmp(log_level, "error") == 0 ) {
            zbc_log_level = ZBC_LOG_ERROR;
        } else if ( strcmp(log_level, "info") == 0 ) {
            zbc_log_level = ZBC_LOG_INFO;
        } else if ( strcmp(log_level, "debug") == 0 ) {
            zbc_log_level = ZBC_LOG_DEBUG;
        } else if ( strcmp(log_level, "vdebug") == 0 ) {
            zbc_log_level = ZBC_LOG_VDEBUG;
        } else {
            fprintf(stderr, "Unknown log level \"%s\"\n",
                    log_level);
        }
    }

    return;

}

/**
 * zbc_disk_type_str - returns a disk type name
 * @type: (IN) ZBC_DT_SCSI, ZBC_DT_ATA, or ZBC_DT_FAKE
 *
 * Returns a string describing the interface type of a disk.
 */
const char *
zbc_disk_type_str(int type)
{

    switch( type ) {
    case ZBC_DT_SCSI:
        return( "SCSI ZBC" );
    case ZBC_DT_ATA:
        return( "ATA ZAC" );
    case ZBC_DT_FAKE:
        return( "Emulated zoned device" );
    case ZBC_DT_BLOCK:
        return( "Zoned block device" );
    }

    return( "Unknown-disk-type" );

}

/**
 * zbc_disk_model_str - returns a disk model name
 * @model: (IN) ZBC_DM_DRIVE_MANAGED, ZBC_DM_HOST_AWARE, or ZBC_DM_HOST_MANAGED
 *
 * Returns a string describing a model type.
 */
const char *
zbc_disk_model_str(int model)
{

    switch( model ) {
    case ZBC_DM_DRIVE_MANAGED:
        return( "Standard/Drive-managed" );
    case ZBC_DM_HOST_AWARE:
        return( "Host-aware" );
    case ZBC_DM_HOST_MANAGED:
        return( "Host-managed" );
    }

    return( "Unknown-model" );

}

/**
 * zbc_zone_type_str - returns a string describing a zone type.
 * @type: (IN)  ZBC_ZT_CONVENTIONAL, ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 *
 * Returns a string describing a zone type.
 */
const char *
zbc_zone_type_str(enum zbc_zone_type type)
{

    switch( type ) {
    case ZBC_ZT_CONVENTIONAL:
        return( "Conventional" );
    case ZBC_ZT_SEQUENTIAL_REQ:
        return( "Sequential-write-required" );
    case ZBC_ZT_SEQUENTIAL_PREF:
        return( "Sequential-write-preferred" );
    }

    return( "Unknown-type" );

}

/**
 * zbc_zone_cond_str - returns a string describing a zone condition.
 * @zone: (IN)  ZBC_ZC_NOT_WP, ZBC_ZC_EMPTY, ZBC_ZC_IMP_OPEN, ZBC_ZC_EXP_OPEN,
 *              ZBC_ZC_CLOSED, ZBC_ZC_RDONLY, ZBC_ZC_FULL or ZBC_ZC_OFFLINE
 *
 * Returns a string describing a zone condition.
 */
const char *
zbc_zone_condition_str(enum zbc_zone_condition cond)
{
    switch( cond ) {
    case ZBC_ZC_NOT_WP:
        return( "Not-write-pointer" );
    case ZBC_ZC_EMPTY:
        return( "Empty" );
    case ZBC_ZC_IMP_OPEN:
        return( "Implicit-open" );
    case ZBC_ZC_EXP_OPEN:
        return( "Explicit-open" );
    case ZBC_ZC_CLOSED:
        return( "Closed" );
    case ZBC_ZC_RDONLY:
        return( "Read-only" );
    case ZBC_ZC_FULL:
        return( "Full" );
    case ZBC_ZC_OFFLINE:
        return( "Offline" );
    }

    return( "Unknown-cond" );

}

/**
 * zbc_errno - returns detailed error report (sense key, sense code and
 *             sense code qualifier) of the last executed command.
 * @dev: (IN) ZBC device handle
 * @err: (OUT) Address where to return the error report
 */
void
zbc_errno(zbc_device_t *dev,
          zbc_errno_t *err)
{

    if ( dev && err ) {
        memcpy(err, &dev->zbd_errno, sizeof(zbc_errno_t));
    }

    return;
}

/**
 * zbc_sk_str - returns a string describing a sense key.
 * @sk:    (IN)  Sense key
 */
const char *
zbc_sk_str(enum zbc_sk sk) {

    int i = 0;

    while ( zbc_sg_sk_list[i].sk != 0 ) {
        if ( sk == zbc_sg_sk_list[i].sk ) {
            return( zbc_sg_sk_list[i].sk_name );
        }
        i++;
    }

    return( "Unknown-sense-key" );

}

/**
 * zbc_asc_ascq_str - returns a string describing a sense code and sense code qualifier.
 * @asc_ascq:    (IN)  Sense code and sense code qualifier
 */
const char *
zbc_asc_ascq_str(enum zbc_asc_ascq asc_ascq) {

    int i = 0;

    while( zbc_sg_asc_ascq_list[i].asc_ascq  != 0 ) {
        if ( asc_ascq == zbc_sg_asc_ascq_list[i].asc_ascq ) {
            return( zbc_sg_asc_ascq_list[i].ascq_name );
        }
        i++;
    }

    return( "Unknown-additional-sense-code-qualifier" );

}

/**
 * zbc_device_is_smr - test if a device is physically an SMR device.
 * @filename:        (IN) path to the device file
 * @info:            (IN) Address where to store the device information
 *
 * Test if a device is physically SMR. This excludes libzbc emulation mode
 * showing a regular block device or regular file as a ZBC host-managed
 * block device. If @info is not NULL and the device is identified as
 * physically being an SMR device, the device information is returned
 * at the address specified by @info.
 *
 * Returns a negative error code if the device test failed. 1 is returned
 * if the device is identified as being an SMR device. Otherwise, 0 is
 * returned. In this case, the application can use stat/fstat to get more
 * details about the device.
 */
int
zbc_device_is_smr(const char *filename,
		  zbc_device_info_t *info)
{
    zbc_device_t *dev;
    int ret = -ENODEV, i;

    if ( ! filename ) {
	return( -EFAULT );
    }

    /* Test all backends until one accepts the drive. */
    for(i = 0; zbc_ops[i]; i++) {
        ret = zbc_ops[i]->zbd_open(filename, O_RDONLY, &dev);
	if ( ret == 0 ) {
	    /* This backend accepted the drive */
            dev->zbd_ops = zbc_ops[i];
	    break;
	}
    }

    if ( dev->zbd_ops ) {
	if ( dev->zbd_ops != &zbc_fake_ops ) {
	    ret = 1;
	    if ( info ) {
		memcpy(info, &dev->zbd_info, sizeof(zbc_device_info_t));
	    }
	} else {
	    ret = 0;
	}
	dev->zbd_ops->zbd_close(dev);
    } else if ( (ret == -ENODEV)
		|| (ret == -ENXIO) ) {
	ret = 0;
    }

    return( ret );

}

/**
 * zbc_open - open a (device)file for ZBC access.
 * @filename:   path to the file to be opened
 * @flags:      open mode: O_RDONLY, O_WRONLY or O_RDWR
 * @dev:        opaque ZBC handle
 *
 * Opens the file pointed to by @filename, and returns a handle to it
 * in @dev if it the file is a device special file for a ZBC-capable
 * device.  If the device does not support ZBC this calls returns -EINVAL.
 * Any other error code returned from open(2) can be returned as well.
 */
int
zbc_open(const char *filename,
         int flags,
         zbc_device_t **pdev)
{
    zbc_device_t *dev = NULL;
    int ret = -ENODEV, i;

    /* Test all backends until one accepts the drive */
    for(i = 0; zbc_ops[i] != NULL; i++) {
        ret = zbc_ops[i]->zbd_open(filename, flags, &dev);
	if ( ret == 0 ) {
	    /* This backend accepted the drive */
            dev->zbd_ops = zbc_ops[i];
	    break;
	}
    }

    if ( ret != 0 ) {
	zbc_error("Open device %s failed %d (%s)\n",
		  filename,
		  ret,
		  strerror(-ret));
    } else {
	*pdev = dev;
    }

    return( ret );

}

/**
 * zbc_close - close a ZBC file handle.
 * @dev:                ZBC device handle to close
 *
 * Performs the equivalent to close(2) for a ZBC handle.  Can return any
 * error that close could return.
 */
int
zbc_close(zbc_device_t *dev)
{
    return( dev->zbd_ops->zbd_close(dev) );
}

/**
 * zbc_get_device_info - report misc device information
 * @dev:                (IN) ZBC device handle to report on
 * @info:               (IN) structure that contains ZBC device information
 *
 * Reports information about a ZBD device.  The @info parameter is used to
 * return a device information structure which must be allocated by the caller.
 *
 * Returns -EFAULT if an invalid NULL pointer was specified.
 */
int
zbc_get_device_info(zbc_device_t *dev,
                    zbc_device_info_t *info)
{
    int ret = -EFAULT;

    if ( dev && info ) {
        memcpy(info, &dev->zbd_info, sizeof(zbc_device_info_t));
        ret = 0;
    }

    return( ret );

}

/**
 * zbc_report_nr_zones - Get number of zones of a ZBC device
 * @dev:                (IN) ZBC device handle to report on
 * @start_lba:          (IN) Start LBA of the first zone looked at
 * @ro:                 (IN) Reporting options (filter)
 * @nr_zones:           (OUT) Address where to return the number of matching zones
 */
int
zbc_report_nr_zones(struct zbc_device *dev,
                    uint64_t start_lba,
                    enum zbc_reporting_options ro,
                    unsigned int *nr_zones)
{

    return( zbc_report_zones(dev, start_lba, ro, NULL, nr_zones) );

}

/**
 * zbc_report_zones - Update a list of zone information
 * @dev:                (IN) ZBC device handle to report on
 * @start_lba:          (IN) Start LBA for the first zone to reported
 * @ro:                 (IN) Reporting options
 * @zones:              (IN) Pointer to array of zone information
 * @nr_zones:           (IN/OUT) Number of zones int the array @zones
 *
 * Update an array of zone information previously obtained using zbc_report_zones,
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
int
zbc_report_zones(struct zbc_device *dev,
                 uint64_t start_lba,
                 enum zbc_reporting_options ro,
                 struct zbc_zone *zones,
                 unsigned int *nr_zones)
{
    int ret = 0;

    if ( (! dev) || (! nr_zones) ) {
        return( -EFAULT );
    }

    if ( ! zones ) {

        /* Get number of zones */
        ret = zbc_do_report_zones(dev, start_lba, ro & (~ZBC_RO_PARTIAL), NULL, NULL, nr_zones);

    } else {

        unsigned int n, z = 0, nz = 0;

        /* Get zones info */
        while( nz < *nr_zones ) {

            n = *nr_zones - nz;
            ret = zbc_do_report_zones(dev, start_lba, ro | ZBC_RO_PARTIAL, NULL, &zones[z], &n);
            if ( ret != 0 ) {
                zbc_error("Get zones from LBA %llu failed %d (%s) %d %d %d\n",
                          (unsigned long long) start_lba,
			  ret, strerror(-ret), n, *nr_zones, nz);
                break;
            }

            if ( n == 0 ) {
                break;
            }

            nz += n;
            z += n;
            start_lba = zones[z - 1].zbz_start + zones[z - 1].zbz_length;
            if ( start_lba >= dev->zbd_info.zbd_logical_blocks ) {
                break;
            }

        }

        if ( ret == 0 ) {
            *nr_zones = nz;
        }

    }

    return( ret );

}

/**
 * zbc_list_zones - report zones for a ZBC device
 * @dev:                (IN) ZBC device handle to report on
 * @start_lba:          (IN) start LBA for the first zone to reported
 * @ro:                 (IN) Reporting options
 * @zones:              (OUT) pointer for reported zones
 * @nr_zones:           (OUT) number of returned zones
 *
 * Reports the number and details of available zones.  The @zones
 * parameter is used to return an array of zones which is allocated using
 * malloc(3) internally and needs to be freed using free(3).  The number
 * of zones in @zones is returned in @nr_zones.
 *
 * Returns -EIO if an error happened when communicating with the device.
 * Returns -ENOMEM if memory could not be allocated for @zones.
 */
int
zbc_list_zones(struct zbc_device *dev,
               uint64_t start_lba,
               enum zbc_reporting_options ro,
               struct zbc_zone **pzones,
               unsigned int *pnr_zones)
{
    zbc_zone_t *zones = NULL;
    unsigned int nr_zones;
    int ret;

    /* Get total number of zones */
    ret = zbc_report_nr_zones(dev, start_lba, ro & (~ZBC_RO_PARTIAL), &nr_zones);
    if ( ret < 0 ) {
        return( ret );
    }

    zbc_debug("Device %s: %d zones\n",
              dev->zbd_filename,
              nr_zones);

    /* Allocate zone array */
    zones = (zbc_zone_t *) malloc(sizeof(zbc_zone_t) * nr_zones);
    if ( ! zones ) {
        zbc_error("No memory\n");
        return( -ENOMEM );
    }
    memset(zones, 0, sizeof(zbc_zone_t) * nr_zones);

    /* Get zones info */
    ret = zbc_report_zones(dev, start_lba, ro & (~ZBC_RO_PARTIAL), zones, &nr_zones);
    if ( ret != 0 ) {
        zbc_error("zbc_report_zones failed %d\n", ret);
        free(zones);
    } else {
        *pzones = zones;
        *pnr_zones = nr_zones;
    }

    return( ret );

}

/**
 * zbc_open_zone - open the zone for a ZBC zone
 * @dev:                (IN) ZBC device handle to reset on
 * @start_lba:          (IN) Start LBA for the zone to be opened or -1 to open all zones
 *
 * Opens the zone for a ZBC zone if @start_lba is a valid zone start LBA.
 * If @start_lba specifies -1, the all zones are opened.
 * The start LBA for a zone is reported by zbc_report_zones().
 *
 * The zone must be of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 * and be in the ZBC_ZC_EMPTY or ZBC_ZC_IMP_OPEN or ZBC_ZC_CLOSED state,
 * otherwise -EINVAL will be returned.  If the zone status is ZBC_ZC_EXP_OPEN or
 * ZBC_ZC_FULL, the zone state doesn't change.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
int
zbc_open_zone(zbc_device_t *dev,
              uint64_t start_lba)
{
    int ret;

    /* Open zone */
    ret = (dev->zbd_ops->zbd_open_zone)(dev, start_lba);
    if ( ret != 0 ) {
        zbc_error("OPEN ZONE command failed\n");
    }

    return( ret );

}

/**
 * zbc_close_zone - close the zone for a ZBC zone
 * @dev:                (IN) ZBC device handle to reset on
 * @start_lba:          (IN) Start LBA for the zone to be closed or -1 to close all zones
 *
 * Closes the zone for a ZBC zone if @start_lba is a valid zone start LBA.
 * If @start_lba specifies -1, the all zones are closed.
 * The start LBA for a zone is reported by zbc_report_zones().
 *
 * The zone must be of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 * and be in the ZBC_ZC_IMP_OPEN or ZBC_ZC_EXP_OPEN state,
 * otherwise an error will be returned. If the zone write pointer is at the start LBA
 * of the zone, the zone state changes to ZBC_ZC_EMPTY. And if the zone status is
 * ZBC_ZC_FULL or ZBC_ZC_CLOSED, the zone state doesn't change.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
int
zbc_close_zone(zbc_device_t *dev,
               uint64_t start_lba)
{
    int ret;

    /* Close zone */
    ret = (dev->zbd_ops->zbd_close_zone)(dev, start_lba);
    if ( ret != 0 ) {
        zbc_error("CLOSE ZONE command failed\n");
    }

    return( ret );

}

/**
 * zbc_finish_zone - finish the zone for a ZBC zone
 * @dev:                (IN) ZBC device handle to reset on
 * @start_lba:          (IN) Start LBA for the zone to be finished or -1 to finish all zones
 *
 * Finishes the zone for a ZBC zone if @start_lba is a valid zone start LBA.
 * If @start_lba specifies -1, the all zones are finished.
 * The start LBA for a zone is reported by zbc_report_zones().
 *
 * The zone must be of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 * and be in the ZBC_ZC_EMPTY or ZBC_ZC_IMP_OPEN or ZBC_ZC_EXP_OPEN or ZBC_ZC_CLOSED state,
 * otherwise -EINVAL will be returned.  If the zone status is ZBC_ZC_FULL,
 * the zone state doesn't change.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
int
zbc_finish_zone(zbc_device_t *dev,
                uint64_t start_lba)
{
    int ret;

    /* Finish zone */
    ret = (dev->zbd_ops->zbd_finish_zone)(dev, start_lba);
    if ( ret != 0 ) {
        zbc_error("FINISH ZONE command failed\n");
    }

    return( ret );

}

/**
 * zbc_reset_write_pointer - reset the write pointer for a ZBC zone
 * @dev:                ZBC device handle to reset on
 * @start_lba:     start LBA for the zone to be reset or -1 to reset all zones
 *
 * Resets the write pointer for a ZBC zone if @start_lba is a valid
 * zone start LBA. If @start_lba specifies -1, the write pointer of all zones
 * is reset. The start LBA for a zone is reported by zbc_report_zones().
 *
 * The zone must be of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 * and be in the ZBC_ZC_OPEN or ZBC_ZC_FULL state, otherwise -EINVAL
 * will be returned.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
int
zbc_reset_write_pointer(zbc_device_t *dev,
                        uint64_t start_lba)
{
    int ret;

    /* Reset write pointer */
    ret = (dev->zbd_ops->zbd_reset_wp)(dev, start_lba);
    if ( ret != 0 ) {
        zbc_error("RESET WRITE POINTER command failed\n");
    }

    return( ret );

}

/**
 * zbc_pread - read from a ZBC device
 * @dev:                (IN) ZBC device handle to read from
 * @zone:               (IN) The zone to read in
 * @buf:                (IN) Caller supplied buffer to read into
 * @lba_count:          (IN) Number of LBAs to read
 * @lba_ofst:           (IN) LBA offset where to start reading in @zone
 *
 * This an the equivalent to pread(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length and I/O offset.
 * It attempts to read in the a number of bytes (@lba_count * logical_block_size)
 * in the zone (@zone) at the offset (@lba_ofst).
 *
 * All errors returned by pread(2) can be returned. On success, the number of
 * logical blocks read is returned.
 */
int32_t
zbc_pread(zbc_device_t *dev,
          zbc_zone_t *zone,
          void *buf,
          uint32_t lba_count,
          uint64_t lba_ofst)
{
    ssize_t ret = -EFAULT;

    if ( dev && zone && buf ) {

	if ( lba_count ) {
	    ret = (dev->zbd_ops->zbd_pread)(dev, zone, buf, lba_count, lba_ofst);
	    if ( ret <= 0 ) {
		zbc_error("Read %u blocks at block %llu + %llu failed %zd (%s)\n",
			  lba_count,
			  (unsigned long long) zbc_zone_start_lba(zone),
			  (unsigned long long) lba_ofst,
			  -ret,
			  strerror(-ret));
	    }
	} else {
	    ret = 0;
	}

    }

    return( ret );

}

/**
 * zbc_pwrite - write to a ZBC device
 * @dev:                (IN) ZBC device handle to write to
 * @zone:               (IN) The zone to write to
 * @buf:                (IN) Caller supplied buffer to write from
 * @lba_count:          (IN) Number of LBAs to write
 * @lba_ofst:           (IN) LBA Offset where to start writing in @zone
 *
 * This an the equivalent to pwrite(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length. It attempts to writes in the
 * zone (@zone) at the offset (@lba_ofst).
 * The disk write pointer may be updated in case of a succesful call, but this function
 * does not updates the write pointer value of @zone.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * logical blocks written is returned.
 */
int32_t
zbc_pwrite(zbc_device_t *dev,
           zbc_zone_t *zone,
           const void *buf,
           uint32_t lba_count,
           uint64_t lba_ofst)
{
    ssize_t ret = -EFAULT;

    if ( dev && zone && buf ) {

	if ( lba_count ) {

	    /* Execute write */
	    ret = (dev->zbd_ops->zbd_pwrite)(dev, zone, buf, lba_count, lba_ofst);
	    if ( ret <= 0 ) {
		zbc_error("Write %u blocks at block %llu + %llu failed %zd (%s)\n",
			  lba_count,
			  (unsigned long long) zbc_zone_start_lba(zone),
			  (unsigned long long) lba_ofst,
			  ret,
			  strerror(-ret));
	    }

	} else {

	    ret = 0;

	}

    }

    return( ret );

}

/**
 * zbc_write - write to a ZBC device
 * @dev:                (IN) ZBC device handle to write to
 * @zone:               (IN) The zone to write to (at the zone write pointer LBA)
 * @buf:                (IN) Caller supplied buffer to write from
 * @lba_count:          (IN) Number of LBAs to write
 *
 * This an the equivalent to write(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length. Instead of writing at
 * the current file offset it writes at the write pointer for the zone
 * identified by @zone, which is advanced if the write operation succeeds.
 * This function thus cannot be used for a conventional zone, which is not
 * a write pointer zone.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * logical blocks written is returned.
 */
int32_t
zbc_write(struct zbc_device *dev,
          struct zbc_zone *zone,
          const void *buf,
          uint32_t lba_count)
{
    int ret = -EINVAL;

    if ( zbc_zone_sequential(zone)
         && (! zbc_zone_full(zone)) ) {

        ret = zbc_pwrite(dev,
                         zone,
                         buf,
                         lba_count,
                         zbc_zone_wp_lba(zone) - zbc_zone_start_lba(zone));
        if ( ret > 0 ) {
            zbc_zone_wp_lba_inc(zone, ret);
        }

    }

    return( ret );

}

/**
 * zbc_flush - flush to a ZBC device cache
 * @dev:                (IN) ZBC device handle to flush
 *
 * This an the equivalent to fsync/fdatasunc but operates at the device cache level.
 */
int
zbc_flush(zbc_device_t *dev)
{

    return( (dev->zbd_ops->zbd_flush)(dev, 0, 0, 0) );

}

/**
 * zbc_set_zones - Configure zones of a "hacked" ZBC device
 * @dev:      (IN) ZBC device handle of the device to configure
 * @conv_sz:  (IN) Size in logical sectors of the space occupied by conventional zones starting at LBA 0. This can be 0.
 * @zone_sz:  (IN) Size in logical sectors of conventional and sequential write required zones. This cannot be 0.
 *
 * This function only affects devices operating with the emulation (fake) backend driver.
 */
int
zbc_set_zones(zbc_device_t *dev,
              uint64_t conv_sz,
              uint64_t zone_sz)
{
    int ret;

    /* Do this only if supported */
    if ( dev->zbd_ops->zbd_set_zones ) {
        ret = (dev->zbd_ops->zbd_set_zones)(dev, conv_sz, zone_sz);
    } else {
        ret = -ENXIO;
    }

    return( ret );

}

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 * @dev:        (IN) ZBC device handle of the device to configure
 * @start_lba:  (IN) The starting LBA of the zone to configure
 * @wp_lba:     (IN) New value of the zone write pointer. If the LBA is not within the zone LBA range,
 *                   the zone write pointer LBA is set to -1 and the zone condition to FULL.
 *
 * This function only affects devices operating with the emulation (fake) backend driver.
 */
int
zbc_set_write_pointer(struct zbc_device *dev,
                      uint64_t start_lba,
                      uint64_t wp_lba)
{
    int ret;

    /* Do this only if supported */
    if ( dev->zbd_ops->zbd_set_wp ) {
        ret = (dev->zbd_ops->zbd_set_wp)(dev, start_lba, wp_lba);
    } else {
        ret = -ENXIO;
    }

    return( ret );

}


