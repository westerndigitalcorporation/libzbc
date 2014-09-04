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

/***** Macro definitions *****/

/**
 * Device type: SCSI or ATA.
 */
enum zbc_dev_type {
    ZBC_DT_SCSI                 = 0x01,
    ZBC_DT_ATA                  = 0x02,
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
    ZBC_DM_STANDARD             = 0x03,
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
    ZBC_ZC_EMPTY                = 0x01,
    ZBC_ZC_OPEN                 = 0x02,
    ZBC_ZC_RDONLY               = 0x0d,
    ZBC_ZC_FULL                 = 0x0e,
    ZBC_ZC_OFFLINE              = 0x0f,
};

/**
 * Report zone reporting options.
 */
enum zbc_reporting_options {
    ZBC_RO_ALL                  = 0x0,
    ZBC_RO_FULL                 = 0x1,
    ZBC_RO_OPEN                 = 0x2,
    ZBC_RO_EMPTY                = 0x3,
    ZBC_RO_RDONLY               = 0x4,
    ZBC_RO_OFFLINE              = 0x5,
    ZBC_RO_RESET                = 0x6,
};

/***** Type definitions *****/

/**
 * Empty forward declaration, structure is private to the library.
 */
struct zbc_device;

/**
 * Zone descriptor (internal).
 */
struct zbc_zone {

    enum zbc_zone_type          zbz_type;
    enum zbc_zone_condition     zbz_condition;
    uint64_t                    zbz_length;
    uint64_t                    zbz_start;
    uint64_t                    zbz_write_pointer;
    bool                        zbz_need_reset;

    char                        __pad[16];

};
typedef struct zbc_zone zbc_zone_t;

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

    char                        __pad[16];

};
typedef struct zbc_device_info zbc_device_info_t;

/***** Library API *****/

/**
 * Set the library log level.
 * log_level can be: "none", "info", "error", "debug" or "vdebug".
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
 * @start_lba:          (IN) Start LBA for the first zone to reported
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
 * @lba_count:          (IN) Size in bytes of @buf
 * @lba_ofst:           (IN) Offset in nb of LBA where to start to read in @zone
 *
 * This an the equivalent to pread(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length and I/O offset.
 *
 * All errors returned by pread(2) can be returned.
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
 * @lba_count:          (IN) Size in bytes of @buf
 * @lba_ofst:                (IN) Offset in number of LBA where to write in the zone.
 *
 * This an the equivalent to pwrite(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length. It attempts to writes in the
 * zone (@zone) at the offset (@lba_ofst).
 * The write pointer is updated in case of a succesful call.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * physical blocks written is returned.
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
 * @lba_count:          (IN) Size in bytes of @buf
 *
 * This an the equivalent to write(2) that operates on a ZBC device handle,
 * and uses LBA addressing for the buffer length.  Instead of writing at
 * the current file offset it writes at the write pointer for the zone
 * identified by @zone, which is advanced by a successful write call.
 *
 * All errors returned by write(2) can be returned. On success, the number of
 * physical blocks written is returned.
 */
static inline int32_t
zbc_write(struct zbc_device *dev,
          struct zbc_zone *zone,
          const void *buf,
          uint32_t lba_count)
{
    return( zbc_pwrite(dev, zone, buf, lba_count, (zone->zbz_write_pointer - zone->zbz_start) ) );
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
 * Some handy macros.
 */
#define zbc_zone_conventional(z)        ((z)->zbz_type == ZBC_ZT_CONVENTIONAL)
#define zbc_zone_sequential_req(z)      ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_REQ)
#define zbc_zone_sequential_pref(z)     ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_PREF)

#define zbc_zone_empty(z)               ((z)->zbz_condition == ZBC_ZC_EMPTY)
#define zbc_zone_open(z)                ((z)->zbz_condition == ZBC_ZC_OPEN)
#define zbc_zone_rdonly(z)              ((z)->zbz_condition == ZBC_ZC_RDONLY)
#define zbc_zone_full(z)                ((z)->zbz_condition == ZBC_ZC_FULL)
#define zbc_zone_offline(z)             ((z)->zbz_condition == ZBC_ZC_OFFLINE)

#define zbc_zone_start_lba(z)           ((unsigned long long)((z)->zbz_start))
#define zbc_zone_length(z)              ((unsigned long long)((z)->zbz_length))
#define zbc_zone_end_lba(z)             (zbc_zone_start_lba(z) + zbc_zone_length(z))
#define zbc_zone_wp_lba(z)              ((unsigned long long)((z)->zbz_write_pointer))

static inline char *
zbc_disk_type_str(int type)
{

    switch( type ) {
    case ZBC_DT_SCSI:
        return( "SCSI ZBC" );
    case ZBC_DT_ATA:
        return( "ATA ZAC" );
    }

    return( "Unknown" );

}

static inline char *
zbc_disk_model_str(int model)
{

    switch( model ) {
    case ZBC_DM_STANDARD:
        return( "Standard" );
    case ZBC_DM_HOST_AWARE:
        return( "Host-aware" );
    case ZBC_DM_HOST_MANAGED:
        return( "Host-managed" );
    }

    return( "Unknown" );

}

/***** Other functions: may go away in the future *****/

/**
 * zbc_inquiry - Get information (model, vendor, ...) from a ZBC device
 * @dev:                (IN) ZBC device handle
 * @out_buf:            (OUT) The data obtained using the INQUIRY command
 *
 * This executes the INQUIRY command on the device and returns inquiry data at @out_buf.
 * @out_buf is allocated internally using malloc(3) and must be freed by the caller using free(3).
 */
extern int
zbc_inquiry(struct zbc_device *dev,
            uint8_t **buf);

/**
 * zbc_set_zones - Configure zones of a "hacked" ZBC device
 * @dev:                (IN) ZBC device handle of the device to configure
 * @conv_sz:            (IN) Size in physical sectors of the conventional zone (zone 0). This can be 0.
 * @seq_sz:             (IN) Size in physical sectors of sequential write required zones. This cannot be 0.
 *
 * This executes the non-standard SET ZONES command to change the zone configuration of a ZBC drive.
 */
extern int
zbc_set_zones(struct zbc_device *dev,
              uint64_t conv_sz,
              uint64_t seq_sz);

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 * @dev:                (IN) ZBC device handle of the device to configure
 * @zone:               (IN) The zone to configure
 * @wp_lba:             (IN) New value of the write pointer (must be at least equal to the zone start LBA
 *                           (zone empty) and at most equal to the zone last LBA plus 1 (zone full).
 *
 * This executes the non-standard SET ZONES command to change the zone configuration of a ZBC drive.
 */
extern int
zbc_set_write_pointer(struct zbc_device *dev,
                      uint64_t start_lba,
                      uint64_t wp_lba);

#endif /* _LIBZBC_H_ */
