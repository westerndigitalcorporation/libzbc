/* SPDX-License-Identifier: BSD-2-Clause */
/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Christoph Hellwig (hch@infradead.org)
 */
#ifndef __LIBZBC_INTERNAL_H__
#define __LIBZBC_INTERNAL_H__

#include "config.h"
#include "libzbc/zbc.h"
#include "zbc_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

/*
 * Memory page size information.
 */
#define PAGE_SIZE	(sysconf(_SC_PAGESIZE))
#define PAGE_MASK	(PAGE_SIZE - 1)

/**
 * Backend driver descriptor.
 */
struct zbc_drv {

	/**
	 * Driver flag.
	 */
	unsigned int	flag;

	/**
	 * Open device.
	 */
	int		(*zbd_open)(const char *, int, struct zbc_device **);

	/**
	 * Close device.
	 */
	int		(*zbd_close)(struct zbc_device *);

	/**
	 * Report a device zone information.
	 */
	int		(*zbd_report_zones)(struct zbc_device *, uint64_t,
					    enum zbc_reporting_options,
					    struct zbc_zone *, unsigned int *);

	/**
	 * Execute a zone operation.
	 */
	int		(*zbd_zone_op)(struct zbc_device *, uint64_t,
				       enum zbc_zone_op, unsigned int);

	/**
	 * Vector read from a ZBC device.
	 */
	ssize_t		(*zbd_preadv)(struct zbc_device *,
				      const struct iovec *, int, uint64_t);

	/**
	 * Vector write to a ZBC device.
	 */
	ssize_t		(*zbd_pwritev)(struct zbc_device *,
				       const struct iovec *, int, uint64_t);

	/**
	 * Flush to a ZBC device cache.
	 */
	int		(*zbd_flush)(struct zbc_device *);

	/**
	 * Change a device zone configuration.
	 * For emulated drives only (optional).
	 */
	int		(*zbd_set_zones)(struct zbc_device *,
					 uint64_t, uint64_t);

	/**
	 * Change a zone write pointer.
	 * For emulated drives only (optional).
	 */
	int		(*zbd_set_wp)(struct zbc_device *,
				      uint64_t, uint64_t);

	/**
	 * Zoned Block Device statistics (optional).
	 */
	int		(*zbd_get_stats)(struct zbc_device *,
					 struct zbc_zoned_blk_dev_stats *);
};

/**
 * Device descriptor.
 */
struct zbc_device {

	/**
	 * Device file path.
	 */
	char			*zbd_filename;

	/**
	 * Device file descriptor.
	 */
	int			zbd_fd;

	/**
	 * File descriptor used for SG_IO. For block devices, this
	 * may be different from zbd_fd.
	 */
	int			zbd_sg_fd;

	/**
	 * Device operations.
	 */
	struct zbc_drv		*zbd_drv;

	/**
	 * Device info.
	 */
	struct zbc_device_info	zbd_info;

	/**
	 * Device open flags.
	 */
	unsigned int		zbd_o_flags;

	/**
	 * Device backend driver flags.
	 */
	unsigned int		zbd_drv_flags;

	/**
	 * Report zone buffer size alignment.
	 */
	size_t			zbd_report_bufsz_mask;

	/**
	 * Report zone buffer size minimum size.
	 */
	size_t			zbd_report_bufsz_min;

};

/**
 * Per-thread local zbc_errno handling.
 */
extern __thread struct zbc_errno zerrno;

static inline void zbc_set_errno(enum zbc_sk sk, enum zbc_asc_ascq asc_ascq)
{
	zerrno.sk = sk;
	zerrno.asc_ascq = asc_ascq;
}
#define zbc_clear_errno()	zbc_set_errno(0, 0)

/**
 * Test if a device is zoned.
 */
#define zbc_dev_model(dev)	((dev)->zbd_info.zbd_model)
#define zbc_dev_is_zoned(dev)	(zbc_dev_model(dev) == ZBC_DM_HOST_MANAGED || \
				 zbc_dev_model(dev) == ZBC_DM_HOST_AWARE)

/**
 * Device open access mode and allowed drivers mask.
 */
#define ZBC_O_MODE_MASK		(O_RDONLY | O_WRONLY | O_RDWR)
#define ZBC_O_DMODE_MASK	(ZBC_O_MODE_MASK | O_DIRECT)
#define ZBC_O_DRV_MASK		(ZBC_O_DRV_BLOCK | ZBC_O_DRV_SCSI | \
				 ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE)
#define ZBC_O_TEST_DRV_MASK	(ZBC_O_DRV_SCSI | ZBC_O_DRV_ATA | \
				 ZBC_O_DRV_FAKE)

/**
 * Test if a device is in test mode.
 */
#ifdef HAVE_DEVTEST
#define zbc_test_mode(dev)	((dev)->zbd_o_flags & ZBC_O_DEVTEST)
#else
#define zbc_test_mode(dev)	(false)
#endif

/**
 * Block device driver (requires kernel support).
 */
extern struct zbc_drv zbc_block_drv;

/**
 * ZAC (ATA) device driver (uses SG_IO).
 */
extern struct zbc_drv zbc_ata_drv;

/**
 * ZBC (SCSI) device driver (uses SG_IO).
 */
extern struct zbc_drv zbc_scsi_drv;

/**
 * ZBC emulation driver (file or block device).
 */
extern struct zbc_drv zbc_fake_drv;

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * Reporting option mask.
 */
#define zbc_ro_mask(ro)		((ro) & 0x3f)

/**
 * Logical block to sector conversion.
 */
#define zbc_dev_sect2lba(dev, sect)	(sect ? zbc_sect2lba(&(dev)->zbd_info, sect) : sect)
#define zbc_dev_lba2sect(dev, lba)	(lba ? zbc_lba2sect(&(dev)->zbd_info, lba) : lba)

/**
 * Check sector alignment to logical block.
 */
#define zbc_dev_sect_laligned(dev, sect)	\
	((((sect) << 9) & ((dev)->zbd_info.zbd_lblock_size - 1)) == 0)

/**
 * Check sector alignment to physical block.
 */
#define zbc_dev_sect_paligned(dev, sect)	\
	((((sect) << 9) & ((dev)->zbd_info.zbd_pblock_size - 1)) == 0)

/**
 * Count total size of vector buffers.
 */
static inline size_t zbc_iov_count(const struct iovec *iov, int iovcnt)
{
	size_t count = 0;
	int i;

	for (i = 0; i < iovcnt; i++)
		count += iov[i].iov_len;

	return count;
}

/**
 * The block backend driver uses the SCSI backend information and
 * some zone operation.
 */
int zbc_scsi_get_zbd_characteristics(struct zbc_device *dev);
int zbc_scsi_zone_op(struct zbc_device *dev, uint64_t start_sector,
		     enum zbc_zone_op op, unsigned int flags);

/**
 * The ATA backend driver may use the SCSI backend I/O functions.
 */
ssize_t zbc_scsi_preadv(struct zbc_device *dev,
			const struct iovec *iov, int iovcnt, uint64_t offset);
ssize_t zbc_scsi_pwritev(struct zbc_device *dev,
			 const struct iovec *iov, int iovcnt, uint64_t offset);
int zbc_scsi_flush(struct zbc_device *dev);

/**
 * Get device capacity information of an ATA device.
 */
int zbc_ata_get_capacity(struct zbc_device *dev);

/**
 * Log levels.
 */
enum {
	ZBC_LOG_NONE = 0,	/* Disable all messages */
	ZBC_LOG_WARNING,	/* Critical errors (invalid drive,...) */
	ZBC_LOG_ERROR,		/* Normal errors (I/O errors etc) */
	ZBC_LOG_INFO,		/* Informational */
	ZBC_LOG_DEBUG,		/* Debug-level messages */
	ZBC_LOG_MAX
};

/**
 * Library log level.
 */
extern int zbc_log_level;

#define zbc_print(stream,format,args...)		\
	do {						\
		fprintf((stream), format, ## args);     \
		fflush(stream);                         \
	} while (0)

/**
 * Log level controlled messages.
 */
#define zbc_print_level(l,stream,format,args...)		\
	do {							\
		if ((l) <= zbc_log_level)			\
			zbc_print((stream), "(libzbc/%d) "	\
				  format,			\
				  getpid(), ## args);		\
	} while (0)

#define zbc_warning(format,args...)	\
	zbc_print_level(ZBC_LOG_WARNING, stderr, "[WARNING] " format, ##args)

#define zbc_error(format,args...)	\
	zbc_print_level(ZBC_LOG_ERROR, stderr, "[ERROR] " format, ##args)

#define zbc_info(format,args...)	\
	zbc_print_level(ZBC_LOG_INFO, stdout, format, ##args)

#define zbc_debug(format,args...)	\
	zbc_print_level(ZBC_LOG_DEBUG, stdout, format, ##args)

#define zbc_panic(format,args...)	\
	do {						\
		zbc_print_level(ZBC_LOG_ERROR,		\
				stderr,			\
				"[PANIC] " format,      \
				##args);                \
		assert(0);                              \
	} while (0)

#define zbc_assert(cond)					\
	do {							\
		if (!(cond))					\
			zbc_panic("Condition %s failed\n",	\
				  # cond);			\
	} while (0)

#endif

/* __LIBZBC_INTERNAL_H__ */
