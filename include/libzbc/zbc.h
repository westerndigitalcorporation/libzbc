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
 * with libzbc. If not, see <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Christoph Hellwig (hch@infradead.org)
 *          Christophe Louargant (christophe.louargant@wdc.com)
 */

#ifndef _LIBZBC_H_
#define _LIBZBC_H_

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

/**
 * zbc_set_log_level - Set the library log level
 * @log_level:	(IN) library log level.
 *
 * Description:
 * Set the library log level using the level name specified by @log_level.
 * Valid level names are: "none", "error", "info", "debug" or "vdebug".
 * The default level is "none".
 */
extern void zbc_set_log_level(char *log_level);

/**
 * enum zbc_zone_type - Zone type
 *
 * Indicates the type of a zone.
 */
enum zbc_zone_type {

	/**
	 * Unknown zone type.
	 */
	ZBC_ZT_UNKNOWN		= 0x00,

	/**
	 * Conventional zone.
	 */
	ZBC_ZT_CONVENTIONAL	= 0x01,

	/**
	 * Sequential write required zone: a write pointer zone
	 * that must be written sequentially (host-managed drives only).
	 */
	ZBC_ZT_SEQUENTIAL_REQ	= 0x02,

	/**
	 * Sequential write preferred zone: a write pointer zone
	 * that can be written randomly (host-aware drives only).
	 */
	ZBC_ZT_SEQUENTIAL_PREF	= 0x03,

};

/**
 * zbc_zone_type_str - returns a string describing a zone type
 * @type:	(IN) Zone type
 *
 * Description:
 * Returns a string describing a zone type.
 */
extern const char *zbc_zone_type_str(enum zbc_zone_type type);

/**
 * enum zbc_zone_condition - Zone condition
 *
 * A zone condition is determined by the zone type and the ZBC zone state
 * machine, i.e. the operations performed on the zone.
 */
enum zbc_zone_condition {

	/**
	 * Not a write pointer zone (i.e. a conventional zone).
	 */
	ZBC_ZC_NOT_WP		= 0x00,

	/**
	 * Empty sequential zone (zone not written too since last reset).
	 */
	ZBC_ZC_EMPTY		= 0x01,

	/**
	 * Implicitely open zone (i.e. a write command was issued to
	 * the zone).
	 */
	ZBC_ZC_IMP_OPEN		= 0x02,

	/**
	 * Explicitly open zone (a write pointer zone was open using
	 * the OPEN ZONE command).
	 */
	ZBC_ZC_EXP_OPEN		= 0x03,

	/**
	 * Closed zone (a write pointer zone that was written to and closed
	 * using the CLOSE ZONE command).
	 */
	ZBC_ZC_CLOSED		= 0x04,

	/**
	 * Read-only zone: any zone that can only be read.
	 */
	ZBC_ZC_RDONLY		= 0x0d,

	/**
	 * Full zone (a write pointer zones only).
	 */
	ZBC_ZC_FULL		= 0x0e,

	/**
	 * Offline zone: unuseable zone.
	 */
	ZBC_ZC_OFFLINE		= 0x0f,

};

/**
 * zbc_zone_cond_str - Returns a string describing a zone condition
 * @cond:	(IN) Zone condition
 *
 * Description:
 * Returns a string describing a zone condition.
 */
extern const char *zbc_zone_condition_str(enum zbc_zone_condition cond);

/**
 * enum zbc_zone_attributes - Zone attributes
 *
 * Defines the attributes of a zone. Attributes validity depend on the
 * zone type and device model.
 */
enum zbc_zone_attributes {

	/**
	 * Reset write pointer recommended: a write pointer zone for which
	 * the device determined that a RESET WRITE POINTER command execution
	 * is recommended. The drive level condition resulting in this
	 * attribute being set depend on the drive model/vendor and not
	 * defined by the ZBC/ZAC specifications.
	 */
	ZBC_ZA_RWP_RECOMMENDED	= 0x0001,

	/**
	 * Non-Sequential Write Resources Active: indicates that a
	 * sequential write preferred zone (host-aware devices only) was
	 * written at a random LBA (not at the write pointer position).
	 * The drive may reset this attribute at any time after the random
	 * write operation completion.
	 */
	ZBC_ZA_NON_SEQ		= 0x0002,
};

/**
 * struct zbc_zone - Zone descriptor data structure.
 *
 * Provide all information of a zone (position and size, condition and
 * attributes). This data structure is updated using the zbc_report_zones
 * function.
 * In order to unifies handling of zone information for devices with different
 * logical block sizes, zone start, length and write pointer position are
 * reported in unit of 512B sectors, regardless of the actual drive logical
 * block size.
 */
struct zbc_zone {

	/**
	 * Zone length in number of 512B sectors.
	 */
	uint64_t		zbz_length;

	/**
	 * First sector of the zone (512B sector unit).
	 */
	uint64_t		zbz_start;

	/**
	 * Zone write pointer sector position (512B sector unit).
	 */
	uint64_t		zbz_write_pointer;

	/**
	 * Zone type (enum zbc_zone_type).
	 */
	uint8_t			zbz_type;

	/**
	 * Zone condition (enum zbc_zone_condition).
	 */
	uint8_t			zbz_condition;

	/**
	 * Zone attributes (enum zbc_zone_attributes).
	 */
	uint8_t			zbz_attributes;

	/**
	 * Padding to 64 Bytes.
	 */
	uint8_t			__pad[5];

};
typedef struct zbc_zone zbc_zone_t;

/**
 * Some handy accessor macros.
 */
#define zbc_zone_type(z)	((int)(z)->zbz_type)
#define zbc_zone_conventional(z) ((z)->zbz_type == ZBC_ZT_CONVENTIONAL)
#define zbc_zone_sequential_req(z) ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_REQ)
#define zbc_zone_sequential_pref(z) ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_PREF)
#define zbc_zone_sequential(z) 	(zbc_zone_sequential_req(z) || \
				 zbc_zone_sequential_pref(z))

#define zbc_zone_condition(z)	((int)(z)->zbz_condition)
#define zbc_zone_not_wp(z)	((z)->zbz_condition == ZBC_ZC_NOT_WP)
#define zbc_zone_empty(z)	((z)->zbz_condition == ZBC_ZC_EMPTY)
#define zbc_zone_imp_open(z)	((z)->zbz_condition == ZBC_ZC_IMP_OPEN)
#define zbc_zone_exp_open(z)	((z)->zbz_condition == ZBC_ZC_EXP_OPEN)
#define zbc_zone_is_open(z)	(zbc_zone_imp_open(z) || \
				 zbc_zone_exp_open(z))
#define zbc_zone_closed(z)	((z)->zbz_condition == ZBC_ZC_CLOSED)
#define zbc_zone_full(z)	((z)->zbz_condition == ZBC_ZC_FULL)
#define zbc_zone_rdonly(z)	((z)->zbz_condition == ZBC_ZC_RDONLY)
#define zbc_zone_offline(z)	((z)->zbz_condition == ZBC_ZC_OFFLINE)

#define zbc_zone_rwp_recommended(z) ((z)->zbz_attributes & \
				     ZBC_ZA_RWP_RECOMMENDED)
#define zbc_zone_non_seq(z)	((z)->zbz_attributes & ZBC_ZA_NON_SEQ)

#define zbc_zone_start(z)	((unsigned long long)(z)->zbz_start)
#define zbc_zone_length(z)	((unsigned long long)(z)->zbz_length)
#define zbc_zone_wp(z)		((unsigned long long)(z)->zbz_write_pointer)

/**
 * Vendor ID string maximum length.
 */
#define ZBC_DEVICE_INFO_LENGTH  32

/**
 * enum zbc_dev_type - Device type definitions.
 *
 * Each type correspond to a different internal backend driver.
 */
enum zbc_dev_type {

	/**
	 * Unknown drive type.
	 */
	ZBC_DT_UNKNOWN	= 0x00,

	/**
	 * Zoned block device (for kernels supporting ZBC/ZAC).
	 */
	ZBC_DT_BLOCK	= 0x01,

	/**
	 * SCSI device.
	 */
	ZBC_DT_SCSI	= 0x02,

	/**
	 * ATA device.
	 */
	ZBC_DT_ATA	= 0x03,

	/**
	 * Fake device (emulation mode).
	 */
	ZBC_DT_FAKE	= 0x04,

};

/**
 * zbc_device_type_str - Returns a device type name
 * @type:	(IN) Device type
 *
 * Description:
 * Returns a string describing the interface type of a device.
 */
extern const char *zbc_device_type_str(enum zbc_dev_type type);

/**
 * Device model.
 *
 * Indicates the ZBC/ZAC device zone model, i.e host-aware, host-managed,
 * or drive-managed. Note that this last model is not handled by libzbc
 * (the device will be treated as a regular block device as it should).
 *   - Host aware: device type 0h & HAW_ZBC bit 1b
 *   - Host managed: device type 14h & HAW_ZBC bit 0b
 *   - Regular: device type 0h (standard block device)
 */
enum zbc_dev_model {

	/**
	 * Unknown drive model.
	 */
	ZBC_DM_DRIVE_UNKNOWN	= 0x00,

	/**
	 * Host-aware drive model: the device type/signature is 0x00
	 * and the ZONED field of the block device characteristics VPD
	 * page B1h is 01b.
	 */
	ZBC_DM_HOST_AWARE	= 0x01,

	/**
	 * Host-managed drive model: the device type/signature is 0x14/0xabcd.
	 */
	ZBC_DM_HOST_MANAGED	= 0x02,

	/**
	 * Drive-managed drive model: the device type/signature is 0x00
	 * and the ZONED field of the block device characteristics VPD
	 * page B1h is 10b.
	 */
	ZBC_DM_DEVICE_MANAGED	= 0x03,

	/**
	 * Standard block device: the device type/signature is 0x00
	 * and the ZONED field of the block device characteristics VPD
	 * page B1h is 00b.
	 */
	ZBC_DM_STANDARD		= 0x04,

};

/**
 * zbc_device_model_str - Returns a device zone model name
 * @model:	(IN) Device model
 *
 * Description:
 * Returns a string describing a device model.
 */
extern const char *zbc_device_model_str(enum zbc_dev_model model);

/**
 * Device handle.
 */
struct zbc_device;

/**
 * enum zbc_dev_flags - Device flags definitions.
 *
 * Defines device information flags.
 */
enum zbc_dev_flags {

	/**
	 * Indicates that a device has unrestricted read operation,
	 * i.e. that read commands spanning a zone write pointer or two
	 * consecutive zones of the same type will not result in an error.
	 */
	ZBC_UNRESTRICTED_READ = 0x01,

};

/**
 * struct zbc_device_info - Device information
 *
 * Provide information on a device open using the zbc_open function.
 */
struct zbc_device_info {

	/**
	 * Device type.
	 */
	enum zbc_dev_type	zbd_type;

	/**
	 * Device model.
	 */
	enum zbc_dev_model	zbd_model;

	/**
	 * Device vendor, model and firmware revision string.
	 */
	char			zbd_vendor_id[ZBC_DEVICE_INFO_LENGTH];

	/**
	 * Device flags (enum zbc_dev_flags).
	 */
	uint32_t		zbd_flags;

	/**
	 * Total number of 512B sectors of the device.
	 */
	uint64_t		zbd_sectors;

	/**
	 * Size in bytes of the device logical blocks.
	 */
	uint32_t		zbd_lblock_size;

	/**
	 * Total number of logical blocks of the device.
	 */
	uint64_t		zbd_lblocks;

	/**
	 * Size in bytes of the device physical blocks.
	 */
	uint32_t		zbd_pblock_size;

	/**
	 * Total number of physical blocks of the device.
	 */
	uint64_t		zbd_pblocks;

	/**
	 * The maximum number of 512B sectors that can be
	 * transferred with a single command to the device.
	 */
	uint64_t		zbd_max_rw_sectors;

	/**
	 * Optimal maximum number of explicitly open sequential write
	 * preferred zones (host-aware device models only).
	 */
	uint32_t		zbd_opt_nr_open_seq_pref;

	/**
	 * Optimal maximum number of sequential write preferred zones
	 * with the ZBC_ZA_NON_SEQ zone attribute set
	 * (host-aware device models only).
	 */
	uint32_t		zbd_opt_nr_non_seq_write_seq_pref;

	/**
	 * Maximum number of explicitly open sequential write required
	 * zones (host-managed device models only).
	 */
	uint32_t		zbd_max_nr_open_seq_req;

};
typedef struct zbc_device_info zbc_device_info_t;

/**
 * Convert LBA value to 512-bytes sector.
 */
#define zbc_lba2sect(info, lba)	(((lba) * (info)->zbd_lblock_size) >> 9)

/**
 * Convert 512-bytes sector value to LBA.
 */
#define zbc_sect2lba(info, sect) (((sect) << 9) / (info)->zbd_lblock_size)

/**
 * SCSI Sense keys.
 *
 * SCSI sense keys inspected in case of command error.
 */
enum zbc_sk {
	ZBC_SK_ILLEGAL_REQUEST	= 0x5,
	ZBC_SK_DATA_PROTECT	= 0x7,
	ZBC_SK_ABORTED_COMMAND	= 0xB,
};

/**
 * SCSI Additional sense codes and additional sense code qualifiers.
 *
 * SCSI Additional sense codes and additional sense code qualifiers
 * inspected in case of command error.
 */
enum zbc_asc_ascq {
	ZBC_ASC_INVALID_FIELD_IN_CDB			= 0x2400,
	ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE	= 0x2100,
	ZBC_ASC_UNALIGNED_WRITE_COMMAND			= 0x2104,
	ZBC_ASC_WRITE_BOUNDARY_VIOLATION		= 0x2105,
	ZBC_ASC_ATTEMPT_TO_READ_INVALID_DATA		= 0x2106,
	ZBC_ASC_READ_BOUNDARY_VIOLATION			= 0x2107,
	ZBC_ASC_ZONE_IS_READ_ONLY			= 0x2708,
	ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES		= 0x550E,
};

/**
 * struct zbc_errno - Detailed error descriptor.
 *
 * Standard and ZBC defined SCSI sense key and additional
 * sense codes are used to describe the error.
 */
struct zbc_errno {

	/**
	 * Sense code.
	 */
	enum zbc_sk		sk;

	/**
	 * Additional sense code and sense code qualifier.
	 */
	enum zbc_asc_ascq	asc_ascq;

};
typedef struct zbc_errno zbc_errno_t;

/**
 * zbc_errno - Get detailed error code of last operation
 * @dev:	(IN) Device handle obtained with zbc_open
 * @err:	(OUT) Address where to return the error report
 *
 * Description:
 * Return at the address specified by @err a detailed error report
 * of the last command execued. The error report is composed of the
 * SCSI sense key, sense code and sense code qualifier.
 * For successsful commands, all three information are set to 0.
 */
extern void zbc_errno(struct zbc_device *dev, struct zbc_errno  *err);

/**
 * zbc_sk_str - Returns a string describing a sense key
 * @sk:		(IN) Sense key
 *
 * Description:
 * Returns a string describing a sense key.
 */
extern const char *zbc_sk_str(enum zbc_sk sk);

/**
 * zbc_asc_ascq_str - Returns a string describing a sense code
 * @asc_ascq:	(IN) Sense code and sense code qualifier
 *
 * Description:
 * Returns a string describing a sense code and sense code qualifier.
 */
extern const char *zbc_asc_ascq_str(enum zbc_asc_ascq asc_ascq);

/**
 * zbc_device_is_zoned - Test if a device is a zoned block device
 * @filename:	(IN) path to the device file
 * @fake:	(IN) If true, also test emulated devices
 * @info:	(IN) Address where to store the device information
 *
 * Description:
 * Test if a device supports the ZBC/ZAC command set. If @fake is false,
 * only test physical devices. Otherwise, also test regular files and
 * regular block devices that may be in use with the fake backend driver
 * to create an emulated host-managed zoned block device.
 * If @info is not NULL and the device is identified as a zoned block device,
 * the device information is returned at the address specified by @info.
 *
 * Returns a negative error code if the device test failed. 1 is returned
 * if the device is identified as a zoned zoned block device. Otherwise, 0
 * is returned.
 */
extern int zbc_device_is_zoned(const char *filename,
			       bool fake,
			       struct zbc_device_info *info);

/**
 * zbc_open - Open a ZBC device
 * @filename:	(IN) Path to a device file
 * @flags:	(IN) Intended access mode: O_RDONLY, O_WRONLY or O_RDWR
 * @dev:	(OUT) Opaque ZBC device handle
 *
 * Description:
 * Opens the device pointed by @filename, and returns a handle to it
 * at the address specified by @dev if the device is a zoned block device
 * supporting the ZBC or ZAC command set. @filename may specify the path to
 * a regular block device file or a regular file to be used with libzbc
 * emulation mode (ZBC_DT_FAKE device type).
 *
 * If the device is not a zoned block device, -ENXIO will be returned.
 * Any other error code returned by open(2) can be returned as well.
 */
extern int zbc_open(const char *filename, int flags, struct zbc_device **dev);

/**
 * zbc_close - Close a ZBC device
 * @dev:	(IN) Device handle obtained with zbc_open
 *
 * Description:
 * Performs the equivalent to close(2) for a ZBC device open using @zbc_open.
 *
 * Can return any error that close(2) may return.
 */
extern int zbc_close(struct zbc_device *dev);

/**
 * zbc_get_device_info - Get a ZBC device information
 * @dev:	(IN) Device handle obtained with zbc_open
 * @info:	(IN) Address of the information structure to fill
 *
 * Description:
 * Get information about an open device. The @info parameter is used to
 * return a device information. @info must be allocated by the caller.
 */
extern void zbc_get_device_info(struct zbc_device *dev,
				struct zbc_device_info *info);

/**
 * zbc_print_device_info - Print a device information
 * @info:	(IN) The information to print
 * @out:	(IN) File stream to print to
 *
 * Print the content of @info to the file stream @out.
 */
extern void zbc_print_device_info(struct zbc_device_info *info, FILE *out);

/**
 * enum zbc_reporting_options - Reporting options
 *
 * Used to filter the zone information returned by the execution of a
 * REPORT ZONES command. Filtering is based on the value of the reporting
 * option and on the condition of the zones at the time of the execution of
 * the REPORT ZONES command.
 *
 * ZBC_RO_PARTIAL is not a filter: this reporting option can be combined
 * (or'ed) with any other filter option to limit the number of reported
 * zone information to the size of the REPORT ZONES command buffer.
 */
enum zbc_reporting_options {

	/**
	 * List all of the zones in the device.
	 */
	ZBC_RO_ALL		= 0x00,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_EMPTY.
	 */
	ZBC_RO_EMPTY		= 0x01,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_IMP_OPEN.
	 */
	 ZBC_RO_IMP_OPEN	= 0x02,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_EXP_OPEN.
	 */
	ZBC_RO_EXP_OPEN		= 0x03,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_CLOSED.
	 */
	ZBC_RO_CLOSED		= 0x04,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_FULL.
	 */
	ZBC_RO_FULL		= 0x05,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_RDONLY.
	 */
	ZBC_RO_RDONLY		= 0x06,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_OFFLINE.
	 */
	ZBC_RO_OFFLINE		= 0x07,

	/* 08h to 0Fh Reserved */

	/**
	 * List the zones with a zone attribute ZBC_ZA_RWP_RECOMMENDED set.
	 */
	ZBC_RO_RWP_RECOMMENDED	= 0x10,

	/**
	 * List the zones with a zone attribute ZBC_ZA_NON_SEQ set.
	 */
	ZBC_RO_NON_SEQ		= 0x11,

	/* 12h to 3Eh Reserved */

	/**
	 * List of the zones with a Zone Condition of ZBC_ZC_NOT_WP.
	 */
	ZBC_RO_NOT_WP		= 0x3f,

	/**
	 * Partial report flag.
	 */
	ZBC_RO_PARTIAL		= 0x80,

};

/**
 * zbc_report_zones - Get zone information
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) Sector from which to report zones.
 * @ro:		(IN) Reporting options
 * @zones:	(IN) Pointer to the array of zone information to fill
 * @nr_zones:	(IN/OUT) Number of zones in the array @zones
 *
 * Description:
 * Get zone information matching the @sector and @ro arguments and return
 * the information obtained in the array @zones and the number of
 * zone information obtained at the address specified by @nr_zones.
 * The array @zones must be allocated by the caller and @nr_zones must point
 * to the size of the allocated array (number of zone information structures
 * in the array). The first zone reported will be the zone containing or
 * after @sector.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
extern int zbc_report_zones(struct zbc_device *dev,
			    uint64_t sector , enum zbc_reporting_options ro,
			    struct zbc_zone *zones, unsigned int *nr_zones);

/**
 * zbc_report_nr_zones - Get the number of zones matches
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) Sector from which to report zones.
 * @ro:		(IN) Reporting options
 * @nr_zones:	(OUT) Address where to return the number of matching zones
 *
 * Description:
 * Similar to @zbc_report_zones, but returns only the number of zones that
 * @zbc_report_zones would have returned. This is useful to determine the
 * total number of zones of a device to allocate an array of zone information
 * structures for use with @zbc_report_zones.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_report_nr_zones(struct zbc_device *dev, uint64_t sector,
				      enum zbc_reporting_options ro,
				      unsigned int *nr_zones)
{
	return zbc_report_zones(dev, sector, ro, NULL, nr_zones);
}

/**
 * zbc_list_zones - Get zone information
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) Sector from which to report zones.
 * @ro:		(IN) Reporting options
 * @zones:	(OUT) The array of zone information filled
 * @nr_zones:	(OUT) Number of zones in the array @zones
 *
 * Description:
 * Similar to @zbc_report_zones, but also allocates an appropriatly sized
 * array of zone information structures and return the address of the array
 * at the address specified by @zones. The size of the array allocated and
 * filled is returned at the address specified by @nr_zones. Freeing of the
 * memory used by the array of zone information strcutrues allocated by this
 * function is the responsability of the caller.
 *
 * Returns -EIO if an error happened when communicating with the device.
 * Returns -ENOMEM if memory could not be allocated for @zones.
 */
extern int zbc_list_zones(struct zbc_device *dev,
			  uint64_t sector, enum zbc_reporting_options ro,
			  struct zbc_zone **zones, unsigned int *nr_zones);

/**
 * enum zbc_zone_op - Zone operation codes
 *
 * Encode the operation to perform on a zone.
 */
enum zbc_zone_op {

	/**
	 * Reset zone write pointer.
	 */
	ZBC_OP_RESET_ZONE	= 0x01,

	/**
	 * Open a zone.
	 */
	ZBC_OP_OPEN_ZONE	= 0x02,

	/**
	 * Close a zone.
	 */
	ZBC_OP_CLOSE_ZONE	= 0x03,

	/**
	 * Finish a zone.
	 */
	ZBC_OP_FINISH_ZONE	= 0x04,

};

/**
 * enum zbc_zone_op_flags - Zone operation flags.
 *
 * Control the behavior of zone operations.
 * flags defined here can be or'ed together and passed to the functions
 * zbc_open_zone, zbc_close_zone, zbc_finish_zone and zbc_reset_zone.
 */
enum zbc_zone_op_flags {

	/**
 	 * Operate on all possible zones.
	 */
	ZBC_OP_ALL_ZONES = 0x0000001,

};

/**
 * zbc_zone_operation - Execute an operation on a zone
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) First sector of the target zone
 * @op:		(IN) The operation to perform
 * @flags:	(IN) Zone operation flags
 *
 * Description:
 * Exexcute an operation on the zone of @dev starting at the sector specified by
 * @sector. The target zone must be a write pointer zone, that is, its type
 * must be ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * The validity of the operation (reset, open, close or finish) depends on the
 * condition of the target zone. See @zbc_reset_zone, @zbc_open_zone,
 * @zbc_close_zone and @zbc_finish_zone for details.
 * If ZBC_OP_ALL_ZONES is set in @flags then @sector is ignored and
 * the operation is executed on all possible zones.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
extern int zbc_zone_operation(struct zbc_device *dev, uint64_t sector,
			      enum zbc_zone_op op, unsigned int flags);

/**
 * zbc_open_zone - Explicitly open a zone
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) First sector of the zone to open
 * @flags:	(IN) Zone operation flags
 *
 * Description:
 * Explicitly open the zone of @dev starting at the sector specified by
 * @sector. The target zone must be a write pointer zone, that is, its type
 * must be ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * The condition of the target zone must be ZBC_ZC_EMPTY, ZBC_ZC_IMP_OPEN or
 * ZBC_ZC_CLOSED. Otherwise, an error will be returned. Opening a zone with
 * the condition ZBC_ZC_EXP_OPEN has no effect (the zone condition is
 * unchanged).
 * If ZBC_OP_ALL_ZONES is set in @flags then @sector is ignored and
 * all possible zones that can be explictly open will be (see ZBC/ZAC
 * specifications regarding the result of such operation).
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_open_zone(struct zbc_device *dev,
				uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_OPEN_ZONE, flags);
}

/**
 * zbc_close_zone - Close an open zone
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) First sector of the zone to close
 * @flags:	(IN) Zone operation flags.
 *
 * Description:
 * Close an implictly or explictly open zone. The zone to close is identified
 * by its first sector specified by @sector. The target zone must be a write
 * pointer zone, that is, of type ZBC_ZT_SEQUENTIAL_REQ or
 * ZBC_ZT_SEQUENTIAL_PREF. Attempting to close a zone that is empty, full or
 * already closed will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in @flags then @sector is ignored and all
 * implicitly and explicitly open zones are closed.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_close_zone(struct zbc_device *dev,
				 uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_CLOSE_ZONE, flags);
}

/**
 * zbc_finish_zone - Finish a write pointer zone
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) First sector of the zone to finish
 * @flags:	(IN) Zone operation flags.
 *
 * Description:
 * Transition a write pointer zone to the full condition. The target zone is
 * identified by its first sector specified by @sector. The target zone must
 * be a write pointer zone, that is, of type ZBC_ZT_SEQUENTIAL_REQ or
 * ZBC_ZT_SEQUENTIAL_PREF. Attempting to finish a zone that is already full
 * will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in @flags then @sector is ignored and all
 * implicitly and explicitly open zones as well as all closed zones are
 * transitioned to the full condition.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_finish_zone(struct zbc_device *dev,
				  uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_FINISH_ZONE, flags);
}

/**
 * zbc_reset_zone - Reset the write pointer of a zone
 * @dev:	(IN) Device handle obtained with zbc_open
 * @sector:	(IN) First sector of the zone to reset
 * @flags:	(IN) Zone operation flags.
 *
 * Description:
 * Resets the write pointer of the zone identified by its first sector
 * specified by @sector. The target zone must be a write pointer zone,
 * that is, of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * Attempting to reset a write pointer zone that is already empty
 * will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in @flags then @sector is ignored and all
 * write pointer zones that are not empty will be resetted.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_reset_zone(struct zbc_device *dev,
				 uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_RESET_ZONE, flags);
}

/**
 * zbc_pread - Read sectors form a device
 * @dev:	(IN) Device handle obtained with zbc_open
 * @buf:        (IN) Caller supplied buffer to read into
 * @count: 	(IN) Number of 512B sectors to read
 * @offset:	(IN) Offset where to start reading (512B sector unit)
 *
 * This an the equivalent of the standard system call pread(2) that
 * operates on a ZBC device handle and uses 512B sector unit addressing
 * for the amount of data and the position on the device of the data to read.
 * Attempting to read data across zone boundaries or after the write pointer
 * position of a write pointer zone is possible only if the device allows
 * unrestricted reads. This is indicated by the device information structure
 * flags field, using the flag ZBC_UNRESTRICTED_READ.
 * The range of 512B sectors to read, starting at @offset and spanning @count
 * 512B sectors must be aligned on logical blocks boundaries. That is, for a
 * 4K logical block size device, @count and @offset must be multiples of 8.
 *
 * Any error returned by pread(2) can be returned. On success, the number of
 * 512B sectors read is returned.
 */
extern ssize_t zbc_pread(struct zbc_device *dev, void *buf,
			 size_t count, uint64_t offset);

/**
 * zbc_pwrite - Write sectors to a device
 * @dev:	(IN) Device handle obtained with zbc_open
 * @buf:        (IN) Caller supplied buffer to write from
 * @count: 	(IN) Number of 512B sectors to write
 * @offset:	(IN) Offset where to start writing (512B sector unit)
 *
 * This an the equivalent of the standard system call pwrite(2) that
 * operates on a ZBC device handle, and uses 512B sector unit addressing
 * for the amount of data and the position on the device of the data to
 * write. On a host-aware device, any range of 512B sector is acceptable.
 * On a host-managed device, the range os sectors to write can span several
 * conventional zones but cannot span conventional and sequential write
 * required zones. When writing to a sequential write required zone, @offset
 * must specify the current write pointer position of the zone.
 * The range of 512B sectors to write, starting at @offset and spanning @count
 * 512B sectors must be aligned on physical blocks boundaries. That is, for a
 * 4K physical block size device, @count and @offset must be multiples of 8.
 *
 * Any error returned by write(2) can be returned. On success, the number of
 * logical blocks written is returned.
 */
extern ssize_t zbc_pwrite(struct zbc_device *dev, const void *buf,
			  size_t count, uint64_t offset);

/**
 * zbc_flush - Flush a device write cache
 * @dev:	(IN) Device handle obtained with zbc_open
 *
 * This an the equivalent to fsync/fdatasunc but operates at the
 * device cache level.
 *
 * Returns 0 on success and -EIO in case of error.
 */
extern int zbc_flush(struct zbc_device *dev);

#endif /* _LIBZBC_H_ */
