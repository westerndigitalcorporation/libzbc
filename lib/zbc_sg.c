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
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 *         Christophe Louargant (christophe.louargant@wdc.com)
 */

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <linux/limits.h>
#include <linux/fs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <assert.h>

#include "zbc.h"
#include "zbc_sg.h"

/**
 * Definition of the commands
 * Each command is defined by 3 fields.
 * It's name, it's length, and it's opcode.
 */
static struct zbc_sg_cmd_s
{
	char		*cdb_cmd_name;
	int		cdb_opcode;
	int		cdb_sa;
	size_t		cdb_length;
	int		dir;

} zbc_sg_cmd_list[ZBC_SG_CMD_NUM] = {

	[ZBC_SG_TEST_UNIT_READY] =
	{
		"TEST UNIT READY",
		ZBC_SG_TEST_UNIT_READY_CDB_OPCODE,
		0,
		ZBC_SG_TEST_UNIT_READY_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_INQUIRY] =
	{
		"INQUIRY",
		ZBC_SG_INQUIRY_CDB_OPCODE,
		0,
		ZBC_SG_INQUIRY_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	[ZBC_SG_READ_CAPACITY] =
	{
		"READ CAPACITY 16",
		ZBC_SG_READ_CAPACITY_CDB_OPCODE,
		ZBC_SG_READ_CAPACITY_CDB_SA,
		ZBC_SG_READ_CAPACITY_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	[ZBC_SG_READ] =
	{
		"READ 16",
		ZBC_SG_READ_CDB_OPCODE,
		0,
		ZBC_SG_READ_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	[ZBC_SG_WRITE] =
	{
		"WRITE 16",
		ZBC_SG_WRITE_CDB_OPCODE,
		0,
		ZBC_SG_WRITE_CDB_LENGTH,
		SG_DXFER_TO_DEV
	},

	[ZBC_SG_SYNC_CACHE] =
	{
		"SYNCHRONIZE CACHE 16",
		ZBC_SG_SYNC_CACHE_CDB_OPCODE,
		0,
		ZBC_SG_SYNC_CACHE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_REPORT_ZONES] =
	{
		"REPORT ZONES",
		ZBC_SG_REPORT_ZONES_CDB_OPCODE,
		ZBC_SG_REPORT_ZONES_CDB_SA,
		ZBC_SG_REPORT_ZONES_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	[ZBC_SG_RESET_ZONE] =
	{
		"RESET WRITE POINTER",
		ZBC_SG_RESET_ZONE_CDB_OPCODE,
		ZBC_SG_RESET_ZONE_CDB_SA,
		ZBC_SG_RESET_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_OPEN_ZONE] =
	{
		"OPEN ZONE",
		ZBC_SG_OPEN_ZONE_CDB_OPCODE,
		ZBC_SG_OPEN_ZONE_CDB_SA,
		ZBC_SG_OPEN_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_CLOSE_ZONE] =
	{
		"CLOSE ZONE",
		ZBC_SG_CLOSE_ZONE_CDB_OPCODE,
		ZBC_SG_CLOSE_ZONE_CDB_SA,
		ZBC_SG_CLOSE_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_FINISH_ZONE] =
	{
		"FINISH ZONE",
		ZBC_SG_FINISH_ZONE_CDB_OPCODE,
		ZBC_SG_FINISH_ZONE_CDB_SA,
		ZBC_SG_FINISH_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_SET_ZONES] =
	{
		"SET ZONES",
		ZBC_SG_SET_ZONES_CDB_OPCODE,
		ZBC_SG_SET_ZONES_CDB_SA,
		ZBC_SG_SET_ZONES_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_SET_WRITE_POINTER] =
	{
		"SET WRITE POINTER",
		ZBC_SG_SET_WRITE_POINTER_CDB_OPCODE,
		ZBC_SG_SET_WRITE_POINTER_CDB_SA,
		ZBC_SG_SET_WRITE_POINTER_CDB_LENGTH,
		SG_DXFER_NONE
	},

	[ZBC_SG_ATA16] =
	{
		"ATA 16",
		ZBC_SG_ATA16_CDB_OPCODE,
		0,
		ZBC_SG_ATA16_CDB_LENGTH,
		0
	}

};

/**
 * Get the system pagesize
 */
static size_t pagesize;	/* use accessor */
static inline size_t sysconf_pagesize(void)
{
    if (!pagesize) {
	pagesize = sysconf(_SC_PAGESIZE);
    }
    return pagesize;
}

/**
 * Get a command name from its operation code in a CDB.
 */
static char *zbc_sg_cmd_name(struct zbc_sg_cmd *cmd)
{

	if (cmd->code >= 0 &&
	    cmd->code < ZBC_SG_CMD_NUM)
		return zbc_sg_cmd_list[cmd->code].cdb_cmd_name;

	return "(UNKNOWN COMMAND)";
}

/**
 * Set ASC, ASCQ.
 */
static void zbc_sg_set_sense(struct zbc_device *dev, struct zbc_sg_cmd *cmd)
{
	unsigned int sense_buf_len = 0;
	uint8_t *sense_buf = NULL;

	if (cmd) {
		sense_buf = cmd->sense_buf;
		sense_buf_len = cmd->io_hdr.sb_len_wr;
	}

	if (sense_buf == NULL ||
	    sense_buf_len < 4) {
		zbc_clear_errno();
		return;
	}

	if ((sense_buf[0] & 0x7F) == 0x72 ||
	    (sense_buf[0] & 0x7F) == 0x73) {
		/* store sense key, ASC/ASCQ */
		zbc_set_errno(sense_buf[1] & 0x0F,
			      ((int)sense_buf[2] << 8) | (int)sense_buf[3]);
		return;
	}

	if (sense_buf_len < 14) {
		zbc_clear_errno();
		return;
	}

	if ((sense_buf[0] & 0x7F) == 0x70 ||
	    (sense_buf[0] & 0x7F) == 0x71) {
		/* store sense key, ASC/ASCQ */
		zbc_set_errno(sense_buf[2] & 0x0F,
			      ((int)sense_buf[12] << 8) | (int)sense_buf[13]);
	}
}

#ifdef SG_FLAG_DIRECT_IO
#define ZBC_SG_FLAG_DIRECT_IO	SG_FLAG_DIRECT_IO
#else
#define ZBC_SG_FLAG_DIRECT_IO	0x01
#endif
#define ZBC_SG_FLAG_Q_AT_TAIL	0x10

/**
 * Initialize a command.
 */
int zbc_sg_cmd_init(struct zbc_device *dev,
		    struct zbc_sg_cmd *cmd, int cmd_code,
		    uint8_t *out_buf, size_t out_bufsz)
{
	zbc_assert(cmd_code >= 0 && cmd_code < ZBC_SG_CMD_NUM);

	/* Set command */
	memset(cmd, 0, sizeof(struct zbc_sg_cmd));
	cmd->code = cmd_code;
	cmd->cdb_sz = zbc_sg_cmd_list[cmd_code].cdb_length;
	zbc_assert(cmd->cdb_sz <= ZBC_SG_CDB_MAX_LENGTH);
	cmd->cdb_opcode = zbc_sg_cmd_list[cmd_code].cdb_opcode;
	cmd->cdb_sa = zbc_sg_cmd_list[cmd_code].cdb_sa;

	if (!out_buf && out_bufsz > 0) {

		/* Allocate a buffer */
		if (posix_memalign((void **) &cmd->out_buf,
				   sysconf_pagesize(), out_bufsz) != 0) {
			zbc_error("No memory for command output buffer (%zu B)\n",
				  out_bufsz);
			return -ENOMEM;
		}

		memset(cmd->out_buf, 0, out_bufsz);
		cmd->out_buf_needfree = 1;

	} else {

		/* Use specified buffer */
		cmd->out_buf = out_buf;

	}

	cmd->out_bufsz = out_bufsz;

	/* Setup SGIO header */
	cmd->io_hdr.interface_id = 'S';
	cmd->io_hdr.timeout = 20000;

	cmd->io_hdr.flags = ZBC_SG_FLAG_Q_AT_TAIL;
	if (dev->zbd_o_flags & ZBC_O_DIRECT && cmd->out_bufsz)
		cmd->io_hdr.flags |= ZBC_SG_FLAG_DIRECT_IO;

	cmd->io_hdr.cmd_len = cmd->cdb_sz;
	cmd->io_hdr.cmdp = &cmd->cdb[0];

	cmd->io_hdr.dxfer_direction = zbc_sg_cmd_list[cmd_code].dir;
	cmd->io_hdr.dxfer_len = cmd->out_bufsz;
	if (cmd->out_bufsz)
		cmd->io_hdr.dxferp = cmd->out_buf;

	cmd->io_hdr.mx_sb_len = ZBC_SG_SENSE_MAX_LENGTH;
	cmd->io_hdr.sbp = cmd->sense_buf;

	return 0;
}

/**
 * Free resources of a command.
 */
void zbc_sg_cmd_destroy(struct zbc_sg_cmd *cmd)
{
	/* Free the command buffer */
        if (cmd->out_buf && cmd->out_buf_needfree) {
		free(cmd->out_buf);
		cmd->out_buf = NULL;
		cmd->out_bufsz = 0;
        }
}

/**
 * Execute a command.
 */
int zbc_sg_cmd_exec(struct zbc_device *dev, struct zbc_sg_cmd *cmd)
{
	int ret;

	if (zbc_log_level >= ZBC_LOG_DEBUG) {
		zbc_debug("%s: Sending command 0x%02x:0x%02x (%s):\n",
			  dev->zbd_filename,
			  cmd->cdb_opcode,
			  cmd->cdb_sa,
			  zbc_sg_cmd_name(cmd));
		zbc_sg_print_bytes(dev, cmd->cdb, cmd->cdb_sz);
	}

	zbc_debug("%s: Execute %s command with buffer of %zu B\n",
		  dev->zbd_filename,
		  (cmd->io_hdr.flags & ZBC_SG_FLAG_DIRECT_IO) ? "direct" : "normal",
		  cmd->out_bufsz);

	/* Send the SG_IO command */
	ret = ioctl(dev->zbd_sg_fd, SG_IO, &cmd->io_hdr);
	if (ret != 0) {
		ret = -errno;
		zbc_debug("%s: SG_IO ioctl failed %d (%s)\n",
			  dev->zbd_filename,
			  errno,
			  strerror(errno));
		return ret;
	}

	/* Reset errno */
	zbc_sg_set_sense(dev, NULL);

	zbc_debug("%s: Command %s done: status 0x%02x (0x%02x), host status "
		  "0x%04x, driver status 0x%04x (flags 0x%04x)\n",
		  dev->zbd_filename,
		  zbc_sg_cmd_name(cmd),
		  (unsigned int)cmd->io_hdr.status,
		  (unsigned int)cmd->io_hdr.masked_status,
		  (unsigned int)cmd->io_hdr.host_status,
		  (unsigned int)zbc_sg_cmd_driver_status(cmd),
		  (unsigned int)zbc_sg_cmd_driver_flags(cmd));

	/* Check status */
	if (cmd->code == ZBC_SG_ATA16 &&
	    (cmd->cdb[2] & (1 << 5))) {

		/* ATA command status */
		if (cmd->io_hdr.status != ZBC_SG_CHECK_CONDITION) {
			zbc_sg_set_sense(dev, cmd);
			return -EIO;
		}

		if (zbc_sg_cmd_driver_status(cmd) == ZBC_SG_DRIVER_SENSE &&
		    cmd->io_hdr.sb_len_wr > 21 &&
		    cmd->sense_buf[21] != 0x50) {
			zbc_sg_set_sense(dev, cmd);
			return -EIO;
		}

		cmd->io_hdr.status = 0;

	}

	if (cmd->io_hdr.status ||
	    (cmd->io_hdr.host_status != ZBC_SG_DID_OK) ||
	    (zbc_sg_cmd_driver_status(cmd) &&
	     (zbc_sg_cmd_driver_status(cmd) != ZBC_SG_DRIVER_SENSE))) {

		if (zbc_log_level >= ZBC_LOG_DEBUG) {

			zbc_error("%s: Command %s failed with status 0x%02x "
				  "(0x%02x), host status 0x%04x, driver status "
				  "0x%04x (flags 0x%04x)\n",
				  dev->zbd_filename,
				  zbc_sg_cmd_name(cmd),
				  (unsigned int)cmd->io_hdr.status,
				  (unsigned int)cmd->io_hdr.masked_status,
				  (unsigned int)cmd->io_hdr.host_status,
				  (unsigned int)zbc_sg_cmd_driver_status(cmd),
				  (unsigned int)zbc_sg_cmd_driver_flags(cmd));

			if (cmd->io_hdr.sb_len_wr) {
				zbc_debug("%s: Sense data (%d B):\n",
					  dev->zbd_filename,
					  cmd->io_hdr.sb_len_wr);
				zbc_sg_print_bytes(dev, cmd->sense_buf,
						   cmd->io_hdr.sb_len_wr);
			} else {
				zbc_debug("%s: No sense data\n",
					  dev->zbd_filename);
			}

		}

		zbc_sg_set_sense(dev, cmd);

		return -EIO;
	}

	if (cmd->io_hdr.resid) {
		zbc_debug("%s: Transfer missing %d B of data\n",
			  dev->zbd_filename,
			  cmd->io_hdr.resid);
		cmd->out_bufsz -= cmd->io_hdr.resid;
	}

	zbc_debug("%s: Command %s executed in %u ms, %zu B transfered\n",
		  dev->zbd_filename,
		  zbc_sg_cmd_name(cmd),
		  cmd->io_hdr.duration,
		  cmd->out_bufsz);

	return 0;
}

/**
 * Get the absolute device path from the device filename.
 */
static void zbc_sg_get_device_path(struct zbc_device *dev, char *path)
{
	struct stat lst;
	char target[PATH_MAX];
	ssize_t len;

	strncpy(path, dev->zbd_filename, PATH_MAX - 1);

	while (1) {
		if (lstat(path, &lst) != 0) {
			zbc_error("%s: lstat %s failed %d (%s)\n",
				  dev->zbd_filename, path,
				  errno, strerror(errno));
			return;
		}

		if (!S_ISLNK(lst.st_mode))
			return;

		len = readlink(path, target, PATH_MAX - 1);
		if (len < 0) {
			zbc_error("%s: readlink failed %d (%s)\n",
				  dev->zbd_filename,
				  errno, strerror(errno));
			return;
		}
		target[len] = '\0';
		strncpy(path, target, PATH_MAX);
	}
}

/**
 * Get a sysfs file integer value.
 */
static int zbc_sg_get_sysfs_val(char *sysfs_path, unsigned long *val)
{
	int ret = -1;
	FILE *f;

	f = fopen(sysfs_path, "r");
	if (f) {
		ret = fscanf(f, "%lu", val);
		if (ret == 1)
			ret = 0;
		fclose(f);
	}

	return ret;
}

/**
 * SG command maximum transfer length in number of 4KB pages.
 * This may limit the SG reported value to a smaller value likely to work
 * with most HBAs.
 */
#define ZBC_SG_MAX_SEGMENTS	256

/**
 * Get the maximum allowed number of memory segments of a command.
 */
static unsigned long zbc_sg_get_max_segments(struct zbc_device *dev)
{
	unsigned long max_segs;
	char path[PATH_MAX];
	char str[128];

	zbc_sg_get_device_path(dev, path);

	snprintf(str, sizeof(str),
		 "/sys/block/%s/queue/max_segments",
		 basename(path));
	if (zbc_sg_get_sysfs_val(str, &max_segs) < 0)
		max_segs = ZBC_SG_MAX_SEGMENTS;

	return max_segs;
}

/**
 * Get the maximum allowed number of bytes of a command.
 */
static unsigned long zbc_sg_get_max_bytes(struct zbc_device *dev)
{
	unsigned long max_bytes;
	char path[PATH_MAX];
	char str[128];

	zbc_sg_get_device_path(dev, path);

	snprintf(str, sizeof(str),
		 "/sys/block/%s/queue/max_sectors_kb",
		 basename(path));
	if (zbc_sg_get_sysfs_val(str, &max_bytes) < 0)
		max_bytes = 0;

	return max_bytes * 1024;
}

/**
 * Get the maximum allowed command blocks for the device.
 */
void zbc_sg_get_max_cmd_blocks(struct zbc_device *dev)
{
	unsigned int max_bytes = 0, max_segs = ZBC_SG_MAX_SEGMENTS;
	size_t pagesize = sysconf_pagesize();
	struct stat st;
	int ret;

	/* Get device stats */
	if (fstat(dev->zbd_sg_fd, &st) < 0) {
		zbc_debug("%s: stat failed %d (%s)\n",
			  dev->zbd_filename,
			  errno, strerror(errno));
		goto out;
	}

	if (S_ISCHR(st.st_mode)) {
		ret = ioctl(dev->zbd_sg_fd, SG_GET_SG_TABLESIZE, &max_segs);
		if (ret != 0) {
			zbc_debug("%s: SG_GET_SG_TABLESIZE ioctl failed %d (%s)\n",
				  dev->zbd_filename,
				  errno,
				  strerror(errno));
			max_segs = ZBC_SG_MAX_SEGMENTS;
		}

		ret = ioctl(dev->zbd_sg_fd, BLKSECTGET, &max_bytes);
		if (ret != 0) {
			zbc_debug("%s: BLKSECTGET ioctl failed %d (%s)\n",
				  dev->zbd_filename,
				  errno,
				  strerror(errno));
			max_bytes = 0;
		}
	} else if (S_ISBLK(st.st_mode)) {
		max_segs = zbc_sg_get_max_segments(dev);
		max_bytes = zbc_sg_get_max_bytes(dev);
	} else {
		/* Use default */
		max_segs = ZBC_SG_MAX_SEGMENTS;
	}

out:
	if (!max_bytes || max_bytes > max_segs * pagesize)
		max_bytes = max_segs * pagesize;
	dev->zbd_info.zbd_max_rw_sectors = max_bytes >> 9;

	zbc_debug("%s: Maximum command data transfer size is %llu sectors\n",
		  dev->zbd_filename,
		  (unsigned long long)dev->zbd_info.zbd_max_rw_sectors);
}

/**
 * Test if unit is ready. This will retry 5 times if the command
 * returns "UNIT ATTENTION".
 */
int zbc_sg_test_unit_ready(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int ret = -EAGAIN, retries = 5;

	while (retries && (ret == -EAGAIN)) {

		retries--;

		/* Intialize command */
		ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_TEST_UNIT_READY,
				      NULL, 0);
		if (ret != 0) {
			zbc_error("%s: init TEST UNIT READY command failed\n",
				  dev->zbd_filename);
			return ret;
		}
		cmd.cdb[0] = ZBC_SG_TEST_UNIT_READY_CDB_OPCODE;

		/* Execute the SG_IO command */
		ret = zbc_sg_cmd_exec(dev, &cmd);
		if ((ret != 0) &&
		    ((cmd.io_hdr.host_status == ZBC_SG_DID_SOFT_ERROR) ||
		     (cmd.io_hdr.sb_len_wr && (cmd.sense_buf[2] == 0x06))) ) {
			zbc_debug("%s: Unit attention required, %d / 5 retries\n",
				  dev->zbd_filename,
				  retries);
			ret = -EAGAIN;
		}

		zbc_sg_cmd_destroy(&cmd);

	}

	if (ret != 0)
		return -ENXIO;

	return 0;
}

/**
 * Set bytes in a command cdb.
 */
void zbc_sg_set_bytes(uint8_t *cmd, void *buf, int bytes)
{
	uint8_t *v = (uint8_t *) buf;
	int i;

	for (i = 0; i < bytes; i++) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		/* The least significant byte is stored last */
		cmd[bytes - i - 1] = v[i];
#else
		/* The most significant byte is stored first */
		cmd[i] = v[i];
#endif
	}
}

/**
 * Get bytes from a command output buffer.
 */
void zbc_sg_get_bytes(uint8_t *val, union converter *conv, int bytes)
{
	uint8_t *v = (uint8_t *) val;
	int i;

	memset(conv, 0, sizeof(union converter));

	for (i = 0; i < bytes; i++) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		conv->val_buf[bytes - i - 1] = v[i];
#else
		conv->val_buf[i] = v[i];
#endif
	}
}

/**
 * Print an array of bytes.
 */
void zbc_sg_print_bytes(struct zbc_device *dev, uint8_t *buf, unsigned int len)
{
	char msg[512];
	unsigned i = 0, j;
	int n;

	zbc_debug("%s: * +==================================\n",
		  dev->zbd_filename);
	zbc_debug("%s: * |Byte |   0  |  1   |  2   |  3   |\n",
		  dev->zbd_filename);
	zbc_debug("%s: * |=====+======+======+======+======+\n",
		  dev->zbd_filename);

	while (i < len) {

		n = sprintf(msg, "%s: * | %3d |", dev->zbd_filename, i);
		for (j = 0; j < 4; j++) {
			if (i < len)
				n += sprintf(msg + n, " 0x%02x |",
					     (unsigned int)buf[i]);
			else
				n += sprintf(msg + n, "      |");
			i++;
		}

		zbc_debug("%s\n", msg);
		if (i + 4 < len)
			zbc_debug("%s: * |=====+======+======+======+======+\n",
				  dev->zbd_filename);
		else
			zbc_debug("%s: * +==================================\n",
				  dev->zbd_filename);
	}
}

