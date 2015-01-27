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
 *          Christophe Louargant (christophe.louargant@hgst.com)
 */

#ifndef _LIBZBC_H_
#define _LIBZBC_H_

/***** Including files *****/

#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

/***** Macro definitions *****/

/**
 * Device type: SCSI, ATA or fake (emulation).
 * Each type correspond to a different internal backend driver.
 */
enum zbc_dev_type {
    ZBC_DT_SCSI                 = 0x01,
    ZBC_DT_ATA                  = 0x02,
    ZBC_DT_FAKE                 = 0x03,
};
/**
 * Device model:
 *   - Host aware: device type 0h & HAW_ZBC bit 1b
 *   - Host managed: device type 14h & HAW_ZBC bit 0b
 *   - Regular: device type 0h (standard block device)
 */
enum zbc_dev_model {
    ZBC_DM_HOST_AWARE           = 0x01,
    ZBC_DM_HOST_MANAGED         = 0x02,
    ZBC_DM_DRIVE_MANAGED        = 0x03,
};

/**
 * Zone type.
 */
enum zbc_zone_type {
    ZBC_ZT_CONVENTIONAL         = 0x01,
    ZBC_ZT_SEQUENTIAL_REQ       = 0x02,
    ZBC_ZT_SEQUENTIAL_PREF      = 0x03,
};

/**
 * Zone condition.
 */
enum zbc_zone_condition {
    ZBC_ZC_NOT_WP               = 0x00,
    ZBC_ZC_EMPTY                = 0x01,
    ZBC_ZC_OPEN                 = 0x02,
    ZBC_ZC_RDONLY               = 0x0d,
    ZBC_ZC_FULL                 = 0x0e,
    ZBC_ZC_OFFLINE              = 0x0f,
};

/**
 * Report zone reporting options: filters zone information
 * returned by the REPORT ZONES command based on the condition
 * of zones.
 */
enum zbc_reporting_options {
    ZBC_RO_ALL                  = 0x0,
    ZBC_RO_FULL                 = 0x1,
    ZBC_RO_OPEN                 = 0x2,
    ZBC_RO_EMPTY                = 0x3,
    ZBC_RO_RDONLY               = 0x4,
    ZBC_RO_OFFLINE              = 0x5,
    ZBC_RO_RESET                = 0x6,
    ZBC_RO_NON_SEQ              = 0x7,
    ZBC_RO_NOT_WP               = 0xf,
};

/***** Type definitions *****/

/**
 * Empty forward declaration, structure is private to the library.
 */
struct zbc_device;

/**
 * Zone descriptor.
 */
struct zbc_zone {

    enum zbc_zone_type          zbz_type;
    enum zbc_zone_condition     zbz_condition;
    uint64_t                    zbz_length;
    uint64_t                    zbz_start;
    uint64_t                    zbz_write_pointer;
    bool			zbz_need_reset;
    bool			zbz_non_seq;

    char                        __pad[12];

};
typedef struct zbc_zone zbc_zone_t;

/**
 * Some handy accessor macros.
 */
#define zbc_zone_conventional(z)        ((z)->zbz_type == ZBC_ZT_CONVENTIONAL)
#define zbc_zone_sequential_req(z)      ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_REQ)
#define zbc_zone_sequential_pref(z)     ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_PREF)

#define zbc_zone_not_wp(z)              ((z)->zbz_condition == ZBC_ZC_NOT_WP)
#define zbc_zone_empty(z)               ((z)->zbz_condition == ZBC_ZC_EMPTY)
#define zbc_zone_open(z)                ((z)->zbz_condition == ZBC_ZC_OPEN)
#define zbc_zone_rdonly(z)              ((z)->zbz_condition == ZBC_ZC_RDONLY)
#define zbc_zone_full(z)                ((z)->zbz_condition == ZBC_ZC_FULL)
#define zbc_zone_offline(z)             ((z)->zbz_condition == ZBC_ZC_OFFLINE)

#define zbc_zone_need_reset(z)          ((z)->zbz_need_reset)
#define zbc_zone_non_seq(z)          	((z)->zbz_non_seq)

#define zbc_zone_start_lba(z)           ((unsigned long long)((z)->zbz_start))
#define zbc_zone_length(z)              ((unsigned long long)((z)->zbz_length))
#define zbc_zone_end_lba(z)             (zbc_zone_start_lba(z) + zbc_zone_length(z))
#define zbc_zone_wp_lba(z)              ((unsigned long long)((z)->zbz_write_pointer))

/**
 * Vendor ID string length.
 */
#define ZBC_DEVICE_INFO_LENGTH  32

/**
 * Misc information about ZBC device.
 */
struct zbc_device_info {

    enum zbc_dev_type           zbd_type;

    enum zbc_dev_model          zbd_model;

    uint32_t                    zbd_logical_block_size;
    uint64_t                    zbd_logical_blocks;

    uint32_t                    zbd_physical_block_size;
    uint64_t                    zbd_physical_blocks;

    char                        zbd_vendor_id[ZBC_DEVICE_INFO_LENGTH];

};
typedef struct zbc_device_info zbc_device_info_t;

/***** Library API *****/

/**
 * Set the library log level.
 * log_level can be: "none", "error", "info", "debug" or "vdebug".
 */
extern void
zbc_set_log_level(char *log_level);

/**
 * zbc_open - open a (device)file for ZBC access.
 * @filename:           (IN) Path to the ZBC device file
 * @flags:              (IN) open mode: O_RDONLY, O_WRONLY or O_RDWR
 * @dev:                (OUT) opaque ZBC handle
 *
 * Opens the file pointed to by @filename, and returns a handle to it
 * in @dev if it the file is a device special file for a ZBC-capable
 * device.  If the device does not support ZBC this calls returns -EINVAL.
 * Any other error code returned from open(2) can be returned as well.
 */
extern int
zbc_open(const char *filename,
         int flags,
         struct zbc_device **dev);

/**
 * zbc_close - close a ZBC file handle.
 * @dev:                (IN) ZBC device handle to close
 *
 * Performs the equivalent to close(2) for a ZBC handle.  Can return any
 * error that close could return.
 */
extern int
zbc_close(struct zbc_device *dev);

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
extern int
zbc_get_device_info(struct zbc_device *dev,
                    struct zbc_device_info *info);

/**
 * zbc_report_zones - Update a list of zone information
 * @dev:                (IN) ZBC device handle to report on
 * @start_lba:          (IN) Start LBA for the first zone to be reported.
 *                           This parameter is ignored for ZAC devices.
 * @ro:                 (IN) Reporting options
 * @zones:              (IN) Pointer to array of zone information
 * @nr_zones:           (IN/OUT) Number of zones int the array @zones
 *
 * Update an array of zone information previously obtained using zbc_report_zones,
 *
 * Returns -EIO if an error happened when communicating to the device.
 */
extern int
zbc_report_zones(struct zbc_device *dev,
                 uint64_t start_lba,
                 enum zbc_reporting_options ro,
                 struct zbc_zone *zones,
                 unsigned int *nr_zones);

/**
 * zbc_report_nr_zones - Get number of zones of a ZBC device
 * @dev:                (IN) ZBC device handle to report on
 * @start_lba:          (IN) Start LBA of the first zone looked at
 * @ro:                 (IN) Reporting options (filter)
 * @nr_zones:           (OUT) Address where to return the number of matching zones
 */
static inline int
zbc_report_nr_zones(struct zbc_device *dev,
                    uint64_t start_lba,
                    enum zbc_reporting_options ro,
                    unsigned int *nr_zones)
{
    return( zbc_report_zones(dev, start_lba, ro, NULL, nr_zones) );
}

/**
 * zbc_list_zones - report zones for a ZBC device
 * @dev:                (IN) ZBC device handle to report on
 * @start_lba:          (IN) start LBA for the first zone to reported
 * @ro:                 (IN) Reporting options
 * @zones:              (OUT) Pointer for reported zones
 * @nr_zones:           (OUT) number of returned zones
 *
 * Reports the number and details of available zones.  The @zones
 * parameter is used to return an array of zones which is allocated using
 * malloc(3) internally and needs to be freed using free(3).  The number
 * of zones in @zones is returned in @nr_zones.
 *
 * Returns -EIO if an error happened when communicating to the device.
 * Returns -ENOMEM if memory could not be allocated for @zones.
 */
extern int
zbc_list_zones(struct zbc_device *dev,
               uint64_t start_lba,
               enum zbc_reporting_options ro,
               struct zbc_zone **zones,
               unsigned int *nr_zones);

/**
 * zbc_reset_write_pointer - reset the write pointer for a ZBC zone
 * @dev:                (IN) ZBC device handle to reset on
 * @start_lba:          (IN) Start LBA for the zone to be reset or -1 to reset all zones
 *
 * Resets the write pointer for a ZBC zone if @start_lba is a valid
 * zone start LBA. If @start_lba specifies -1, the write pointer of all zones
 * is reset. The start LBA for a zone is reported by zbc_report_zones().
 *
 * The zone must be of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 * and be in the ZBC_ZC_OPEN or ZBC_ZC_FULL state, otherwise -EINVAL
 * will be returned.
 *
 * Returns -EIO if an error happened when communicating to the device.
 */
extern int
zbc_reset_write_pointer(struct zbc_device *dev,
                        uint64_t start_lba);

/**
 * zbc_read - read from a ZBC device
 * @dev:                (IN) ZBC device handle to read from
 * @zone:               (IN) The zone to read in
 * @buf:                (IN) Caller supplied buffer to read into
 * @lba_count:          (IN) Number of LBAs to read
 * @lba_ofst:           (IN) LBA offset where to start reading in @zone
 *
 * This an the equivalent to pread(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length and I/O offset.
 *
 * All errors returned by pread(2) can be returned. On success, the number of
 * logical blocks read is returned.
 */
extern int32_t
zbc_pread(struct zbc_device *dev,
          struct zbc_zone *zone,
          void *buf,
          uint32_t lba_count,
          uint64_t lba_ofst);

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
 * The write pointer is updated in case of a succesful call.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * logical blocks written is returned.
 */
extern int32_t
zbc_pwrite(struct zbc_device *dev,
           struct zbc_zone *zone,
           const void *buf,
           uint32_t lba_count,
           uint64_t lba_ofst);

/**
 * zbc_write - write to a ZBC device
 * @dev:                (IN) ZBC device handle to write to
 * @zone:               (IN) The zone to write to (at the zone write pointer LBA)
 * @buf:                (IN) Caller supplied buffer to write from
 * @lba_count:          (IN) Number of LBAs to write
 *
 * This an the equivalent to write(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length.  Instead of writing at
 * the current file offset it writes at the write pointer for the zone
 * identified by @zone, which is advanced by a successful write call.
 * This function thus cannot be used for a conventional zone, which is not
 * a write pointer zone.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * logical blocks written is returned.
 */
static inline int32_t
zbc_write(struct zbc_device *dev,
          struct zbc_zone *zone,
          const void *buf,
          uint32_t lba_count)
{

    if ( zbc_zone_conventional(zone) ) {
	return( -EINVAL );
    }

    return( zbc_pwrite(dev, zone, buf, lba_count, (zone->zbz_write_pointer - zone->zbz_start)) );

}

/**
 * zbc_flush - flush to a ZBC device cache
 * @dev:                (IN) ZBC device handle to flush
 *
 * This an the equivalent to fsync/fdatasunc but operates at the device cache level.
 */
extern int
zbc_flush(struct zbc_device *dev);

/**
 * Returns a disk type name.
 */
static inline const char *
zbc_disk_type_str(int type)
{

    switch( type ) {
    case ZBC_DT_SCSI:
        return( "SCSI ZBC" );
    case ZBC_DT_ATA:
        return( "ATA ZAC" );
    case ZBC_DT_FAKE:
        return( "Emulated zoned device" );
    }

    return( "Unknown" );

}

/**
 * Returns a disk model name.
 */
static inline const char *
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

    return( "Unknown" );

}

#endif /* _LIBZBC_H_ */
