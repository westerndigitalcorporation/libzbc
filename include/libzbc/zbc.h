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
 * zbc_open flag to force the use of ATA read and write
 * commands through ATA16 SCSI command (ATA PASSTHROUGH).
 * Otherwise, for ATA drives, native SCSI read and write
 * commands are used and either the kernel or the drive HBA
 * translaet these command to ATA commands.
 * This flag is ignored if the target device is not an ATA device.
 *
 * This is defined as bit 30 of the standard fcntl flags.
 */
#define ZBC_FORCE_ATA_RW       	0x40000000

/**
 * Device info flags.
 *
 * ZBC_UNRESTRICTED_READ: indicates that a device has unrestricted
 *                        read operation, i.e. that read commands
 *                        spanning a zone write pointer or 2 zones
 *                        of the same type will not result in an error.
 */
#define ZBC_UNRESTRICTED_READ   0x00000001

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
    ZBC_DM_DRIVE_UNKNOWN        = 0x00,
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
    ZBC_ZC_IMP_OPEN             = 0x02,
    ZBC_ZC_EXP_OPEN             = 0x03,
    ZBC_ZC_CLOSED               = 0x04,
    ZBC_ZC_RDONLY               = 0x0d,
    ZBC_ZC_FULL                 = 0x0e,
    ZBC_ZC_OFFLINE              = 0x0f,
};

/**
 * Zone flags: need reset, and non-seq write.
 */
enum zbc_zone_flags {
    ZBC_ZF_NEED_RESET           = 0x0001,
    ZBC_ZF_NON_SEQ              = 0x0002,
};

/**
 * Report zone reporting options: filters zone information
 * returned by the REPORT ZONES command based on the condition
 * of zones. Note that ZBC_RO_PARTIAL is not a filter: this
 * option can be combined (or'ed) with any other option to limit
 * the number of reported zone information to the size of the
 * REPORT ZONE command buffer.
 */
enum zbc_reporting_options {
    ZBC_RO_ALL                  = 0x00,
    ZBC_RO_EMPTY                = 0x01,
    ZBC_RO_IMP_OPEN             = 0x02,
    ZBC_RO_EXP_OPEN             = 0x03,
    ZBC_RO_CLOSED               = 0x04,
    ZBC_RO_FULL                 = 0x05,
    ZBC_RO_RDONLY               = 0x06,
    ZBC_RO_OFFLINE              = 0x07,
    ZBC_RO_RESET                = 0x10,
    ZBC_RO_NON_SEQ              = 0x11,
    ZBC_RO_NOT_WP               = 0x3f,
    ZBC_RO_PARTIAL              = 0x80,
};

/**
 * Sense key.
 */
enum zbc_sk {
    ZBC_E_ILLEGAL_REQUEST         = 0x5,
    ZBC_E_DATA_PROTECT            = 0x7,
    ZBC_E_ABORTED_COMMAND         = 0xB,
};

/**
 * Additional sense code/Additional sense code qualifier.
 */
enum zbc_asc_ascq {
    ZBC_E_INVALID_FIELD_IN_CDB                  = 0x2400,
    ZBC_E_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE    = 0x2100,
    ZBC_E_UNALIGNED_WRITE_COMMAND               = 0x2104,
    ZBC_E_WRITE_BOUNDARY_VIOLATION              = 0x2105,
    ZBC_E_ATTEMPT_TO_READ_INVALID_DATA          = 0x2106,
    ZBC_E_READ_BOUNDARY_VIOLATION               = 0x2107,
    ZBC_E_ZONE_IS_READ_ONLY                     = 0x2708,
    ZBC_E_INSUFFICIENT_ZONE_RESOURCES           = 0x550E,
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

    uint64_t                    zbz_length;
    uint64_t                    zbz_start;
    uint64_t                    zbz_write_pointer;

    uint8_t                     zbz_type;
    uint8_t                     zbz_condition;
    uint8_t                     zbz_flags;

    uint8_t                     __pad[5];

};
typedef struct zbc_zone zbc_zone_t;

/**
 * Detailed error descriptor.
 * Standard and ZBC defined SCSI sense key and additional
 * sense codes are used to describe the error.
 */
struct zbc_errno {

    enum zbc_sk                 sk;
    enum zbc_asc_ascq           asc_ascq;

};
typedef struct zbc_errno zbc_errno_t;

/**
 * Some handy accessor macros.
 */
#define zbc_zone_type(z)                ((int)(z)->zbz_type)
#define zbc_zone_conventional(z)        ((z)->zbz_type == ZBC_ZT_CONVENTIONAL)
#define zbc_zone_sequential_req(z)      ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_REQ)
#define zbc_zone_sequential_pref(z)     ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_PREF)
#define zbc_zone_sequential(z)     	(zbc_zone_sequential_req(z) || zbc_zone_sequential_pref(z))

#define zbc_zone_condition(z)           ((int)(z)->zbz_condition)
#define zbc_zone_not_wp(z)              ((z)->zbz_condition == ZBC_ZC_NOT_WP)
#define zbc_zone_empty(z)               ((z)->zbz_condition == ZBC_ZC_EMPTY)
#define zbc_zone_imp_open(z)            ((z)->zbz_condition == ZBC_ZC_IMP_OPEN)
#define zbc_zone_exp_open(z)            ((z)->zbz_condition == ZBC_ZC_EXP_OPEN)
#define zbc_zone_is_open(z)             (zbc_zone_imp_open(z) || zbc_zone_exp_open(z))
#define zbc_zone_closed(z)              ((z)->zbz_condition == ZBC_ZC_CLOSED)
#define zbc_zone_full(z)                ((z)->zbz_condition == ZBC_ZC_FULL)
#define zbc_zone_rdonly(z)              ((z)->zbz_condition == ZBC_ZC_RDONLY)
#define zbc_zone_offline(z)             ((z)->zbz_condition == ZBC_ZC_OFFLINE)

#define zbc_zone_need_reset(z)          (((z)->zbz_flags & ZBC_ZF_NEED_RESET) != 0)
#define zbc_zone_non_seq(z)          	(((z)->zbz_flags & ZBC_ZF_NON_SEQ) != 0)

#define zbc_zone_start_lba(z)           ((unsigned long long)((z)->zbz_start))
#define zbc_zone_length(z)              ((unsigned long long)((z)->zbz_length))
#define zbc_zone_next_lba(z)            (zbc_zone_start_lba(z) + zbc_zone_length(z))
#define zbc_zone_last_lba(z)            (zbc_zone_next_lba(z) - 1)
#define zbc_zone_wp_lba(z)              ((unsigned long long)((z)->zbz_write_pointer))

#define zbc_zone_wp_within_zone(z)      ((zbc_zone_wp_lba(z) >= zbc_zone_start_lba(z)) \
                                         && (zbc_zone_wp_lba(z) <= zbc_zone_last_lba(z)))

#define zbc_zone_wp_lba_reset(z)                        	\
    do {                                                	\
        if ( zbc_zone_sequential(z) ) {                         \
            (z)->zbz_write_pointer = zbc_zone_start_lba(z); 	\
            (z)->zbz_condition = ZBC_ZC_EMPTY;                  \
        }                                                       \
    } while( 0 )

#define zbc_zone_wp_lba_inc(z, count)                           \
    do {                                                        \
        if ( zbc_zone_sequential(z) ) {                         \
            (z)->zbz_write_pointer += (count);                  \
            if ( zbc_zone_wp_lba(z) > zbc_zone_last_lba(z) ) {	\
                (z)->zbz_write_pointer = zbc_zone_next_lba(z);  \
                (z)->zbz_condition = ZBC_ZC_FULL;               \
            }                                                   \
        }                                                       \
    } while( 0 )

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

    char                        zbd_vendor_id[ZBC_DEVICE_INFO_LENGTH];

    uint32_t                    zbd_logical_block_size;
    uint64_t                    zbd_logical_blocks;

    uint32_t                    zbd_physical_block_size;
    uint64_t                    zbd_physical_blocks;

    uint32_t                    zbd_opt_nr_open_seq_pref;
    uint32_t                    zbd_opt_nr_non_seq_write_seq_pref;
    uint32_t                    zbd_max_nr_open_seq_req;

    uint32_t                    zbd_flags;

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
 * Returns -EIO if an error happened when communicating with the device.
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
extern int
zbc_report_nr_zones(struct zbc_device *dev,
                    uint64_t start_lba,
                    enum zbc_reporting_options ro,
                    unsigned int *nr_zones);

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
 * Returns -EIO if an error happened when communicating with the device.
 * Returns -ENOMEM if memory could not be allocated for @zones.
 */
extern int
zbc_list_zones(struct zbc_device *dev,
               uint64_t start_lba,
               enum zbc_reporting_options ro,
               struct zbc_zone **zones,
               unsigned int *nr_zones);

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
 * and be in the ZBC_ZC_EMPTY or ZBC_ZC_IMP_OPEN or ZBC_ZC_CLOSED or ZBC_ZC_EXP_OPEN state,
 * otherwise -EINVAL will be returned.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
extern int
zbc_open_zone(struct zbc_device *dev,
              uint64_t start_lba);

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
 * and be in the ZBC_ZC_IMP_OPEN or ZBC_ZC_EXP_OPEN or ZBC_ZC_CLOSED or ZBC_ZC_FULL state,
 * otherwise -EINVAL will be returned. If write pointer is at start LBA of the zone,
 * the zone state changes to ZBC_ZC_EMPTY. And if the zone status is ZBC_ZC_FULL,
 * the zone state doesn't change.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
extern int
zbc_close_zone(struct zbc_device *dev,
               uint64_t start_lba);

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
 * and be in the ZBC_ZC_IMP_OPEN or ZBC_ZC_EXP_OPEN or ZBC_ZC_CLOSED state,
 * otherwise -EINVAL will be returned. If zone is in ZBC_ZC_CLOSED state,
 * the zone state changes to ZBC_ZC_IMP_OPEN exceptionally.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
extern int
zbc_finish_zone(struct zbc_device *dev,
                uint64_t start_lba);

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
 * Returns -EIO if an error happened when communicating with the device.
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
 * The disk write pointer may be updated in case of a succesful call, but this function
 * does not updates the write pointer value of @zone.
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
 * and uses LBA addressing for the buffer length. Instead of writing at
 * the current file offset it writes at the write pointer for the zone
 * identified by @zone, which is advanced if the write operation succeeds.
 * This function thus cannot be used for a conventional zone, which is not
 * a write pointer zone.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * logical blocks written is returned.
 */
extern int32_t
zbc_write(struct zbc_device *dev,
          struct zbc_zone *zone,
          const void *buf,
          uint32_t lba_count);

/**
 * zbc_flush - flush to a ZBC device cache
 * @dev:                (IN) ZBC device handle to flush
 *
 * This an the equivalent to fsync/fdatasunc but operates at the device cache level.
 */
extern int
zbc_flush(struct zbc_device *dev);

/**
 * zbc_disk_type_str - returns a disk type name
 * @type: (IN) ZBC_DT_SCSI, ZBC_DT_ATA, or ZBC_DT_FAKE
 *
 * Returns a string describing the interface type of a disk.
 */
extern const char *
zbc_disk_type_str(int type);

/**
 * zbc_disk_model_str - returns a disk model name
 * @model: (IN) ZBC_DM_DRIVE_MANAGED, ZBC_DM_HOST_AWARE, or ZBC_DM_HOST_MANAGED
 *
 * Returns a string describing a model type.
 */
extern const char *
zbc_disk_model_str(int model);

/**
 * zbc_zone_type_str - returns a string describing a zone type.
 * @type: (IN)  ZBC_ZT_CONVENTIONAL, ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF
 *
 * Returns a string describing a zone type.
 */
extern const char *
zbc_zone_type_str(enum zbc_zone_type type);

/**
 * zbc_zone_cond_str - returns a string describing a zone condition.
 * @cond: (IN)  ZBC_ZC_NOT_WP, ZBC_ZC_EMPTY, ZBC_ZC_IMP_OPEN, ZBC_ZC_EXP_OPEN,
 *              ZBC_ZC_CLOSED, ZBC_ZC_RDONLY, ZBC_ZC_FULL or ZBC_ZC_OFFLINE
 *
 * Returns a string describing a zone condition.
 */
extern const char *
zbc_zone_condition_str(enum zbc_zone_condition cond);

/**
 * zbc_errno - returns detailed error report (sense key, sense code and sense code qualifier) of the last executed command.
 * @dev: (IN) ZBC device handle
 * @err: (OUT) Address where to return the error report
 */
extern void
zbc_errno(struct zbc_device *dev,
          struct zbc_errno  *err);

/**
 * zbc_sk_str - returns a string describing a sense key.
 * @sk:    (IN)  Sense key
 */
extern const char *
zbc_sk_str(enum zbc_sk sk);

/**
 * zbc_asc_ascq_str - returns a string describing a sense code and sense code qualifier.
 * @asc_ascq:    (IN)  Sense code and sense code qualifier
 */
extern const char *
zbc_asc_ascq_str(enum zbc_asc_ascq asc_ascq);

#endif /* _LIBZBC_H_ */
