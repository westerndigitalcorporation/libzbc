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
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Christoph Hellwig (hch@infradead.org)
 */

#ifndef __LIBZBC_INTERNAL_H__
#define __LIBZBC_INTERNAL_H__

#include "config.h"
#include "libzbc/zbc.h"
#include "zbc_log.h"

#include <stdlib.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

/**
 * Zone operation code.
 */
enum zbc_zone_op {
	ZBC_OP_OPEN_ZONE = 0x01,
	ZBC_OP_CLOSE_ZONE = 0x02,
	ZBC_OP_FINISH_ZONE = 0x03,
	ZBC_OP_RESET_ZONE = 0x04,
};

/**
 * Device operations.
 */
typedef struct zbc_ops {

	/**
	 * Open device.
	 */
	int	(*zbd_open)(const char *, int, struct zbc_device **);

	/**
	 * Close device.
	 */
	int	(*zbd_close)(struct zbc_device *);

	/**
	 * Read from a ZBC device
	 */
	ssize_t	(*zbd_pread)(struct zbc_device *, void *, size_t, uint64_t);

	/**
	 * Write to a ZBC device
	 */
	ssize_t	(*zbd_pwrite)(struct zbc_device *, const void *,
			      size_t, uint64_t);

	/**
	 * Flush to a ZBC device cache.
	 */
	int	(*zbd_flush)(struct zbc_device *, uint64_t, size_t, int);

	/**
	 * Report a device zone information (mandatory).
	 */
	int	(*zbd_report_zones)(struct zbc_device *, uint64_t,
                                    enum zbc_reporting_options,
				    zbc_zone_t *, unsigned int *);

	/**
	 * Open a zone or all zones (mandatory).
	 */
	int	(*zbd_open_zone)(struct zbc_device *, uint64_t, unsigned int);

	/**
	 * Close a zone or all zones (mandatory).
	 */
	int	(*zbd_close_zone)(struct zbc_device *, uint64_t, unsigned int);

	/**
	 * Finish a zone or all zones (mandatory).
	 */
	int	(*zbd_finish_zone)(struct zbc_device *, uint64_t, unsigned int);

	/**
	 * Reset a zone or all write pointer zones (mandatory).
	 */
	int	(*zbd_reset_zone)(struct zbc_device *, uint64_t, unsigned int);

	/**
	 * Change a device zone configuration.
	 * For emulated drives only (optional).
	 */
	int	(*zbd_set_zones)(struct zbc_device *, uint64_t, uint64_t);

	/**
	 * Change a zone write pointer.
	 * For emulated drives only (optional).
	 */
	int	(*zbd_set_wp)(struct zbc_device *, uint64_t, uint64_t);

} zbc_ops_t;

/**
 * Device descriptor.
 */
typedef struct zbc_device {

	/**
	 * Device file path.
	 */
	char			*zbd_filename;

	/**
	 * Device file descriptor.
	 */
	int			zbd_fd;

	/**
	 * Device operations.
	 */
	zbc_ops_t		*zbd_ops;

	/**
	 * Device info.
	 */
	zbc_device_info_t	zbd_info;

	/**
	 * Device flags set by backend drivers.
	 */
	unsigned int		zbd_flags;

	/**
	 * Command execution error info.
	 */
	zbc_errno_t		zbd_errno;

} zbc_device_t;

/**
 * Block device operations (requires kernel support).
 */
extern zbc_ops_t zbc_block_ops;

/**
 * ZAC (ATA) device operations (uses SG_IO).
 */
extern zbc_ops_t zbc_ata_ops;

/**
 * ZBC (SCSI) device operations (uses SG_IO).
 */
extern zbc_ops_t zbc_scsi_ops;

/**
 * ZBC emulation (file or block device).
 */
extern struct zbc_ops zbc_fake_ops;

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * Reporting option mask.
 */
#define zbc_ro_mask(ro)		((ro) & 0x3f)

/**
 * Logical block/sector conversion.
 */
#define zbc_dev_sect2lba(dev, sector)	zbc_sect2lba(&(dev)->zbd_info, sector)
#define zbc_dev_lba2sect(dev, lba)	zbc_lba2sect(&(dev)->zbd_info, lba)

#define zbc_dev_bytes2lba(dev, bytes) \
	((bytes) / (dev)->zbd_info.zbd_lblock_size)
#define zbc_dev_lba2bytes(dev, lba) \
	((lba) * (dev)->zbd_info.zbd_lblock_size)
/**
 * SCSI backend driver operations are also used
 * for block device control.
 */
extern int zbc_scsi_get_zbd_chars(zbc_device_t *dev);

/**
 * Zone operation.
 */
extern int zbc_scsi_zone_op(zbc_device_t *dev, enum zbc_zone_op op,
			    uint64_t start_lba, unsigned int flags);

/**
 * Read from a device.
 */
extern ssize_t zbc_scsi_pread(zbc_device_t *dev, void *buf,
			      size_t lba_count, uint64_t lba_offset);

/**
 * Write to a ZBC device
 */
extern ssize_t zbc_scsi_pwrite(zbc_device_t *dev, const void *buf,
			       size_t lba_count, uint64_t lba_offset);

#endif

/* __LIBZBC_INTERNAL_H__ */
