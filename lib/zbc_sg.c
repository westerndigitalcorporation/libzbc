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

/***** Including files *****/

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <assert.h>

#include "zbc.h"
#include "zbc_sg.h"

/***** Private data *****/

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

	/* ZBC_SG_TEST_UNIT_READY */
	{
		"TEST UNIT READY",
		ZBC_SG_TEST_UNIT_READY_CDB_OPCODE,
		0,
		ZBC_SG_TEST_UNIT_READY_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_INQUIRY */
	{
		"INQUIRY",
		ZBC_SG_INQUIRY_CDB_OPCODE,
		0,
		ZBC_SG_INQUIRY_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	/* ZBC_SG_READ_CAPACITY */
	{
		"READ CAPACITY 16",
		ZBC_SG_READ_CAPACITY_CDB_OPCODE,
		ZBC_SG_READ_CAPACITY_CDB_SA,
		ZBC_SG_READ_CAPACITY_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	/* ZBC_SG_READ */
	{
		"READ 16",
		ZBC_SG_READ_CDB_OPCODE,
		0,
		ZBC_SG_READ_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	/* ZBC_SG_WRITE */
	{
		"WRITE 16",
		ZBC_SG_WRITE_CDB_OPCODE,
		0,
		ZBC_SG_WRITE_CDB_LENGTH,
		SG_DXFER_TO_DEV
	},

	/* ZBC_SG_SYNC_CACHE */
	{
		"SYNCHRONIZE CACHE 16",
		ZBC_SG_SYNC_CACHE_CDB_OPCODE,
		0,
		ZBC_SG_SYNC_CACHE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_REPORT_ZONES */
	{
		"REPORT ZONES",
		ZBC_SG_REPORT_ZONES_CDB_OPCODE,
		ZBC_SG_REPORT_ZONES_CDB_SA,
		ZBC_SG_REPORT_ZONES_CDB_LENGTH,
		SG_DXFER_FROM_DEV
	},

	/* ZBC_SG_OPEN_ZONE */
	{
		"OPEN ZONE",
		ZBC_SG_OPEN_ZONE_CDB_OPCODE,
		ZBC_SG_OPEN_ZONE_CDB_SA,
		ZBC_SG_OPEN_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_CLOSE_ZONE */
	{
		"CLOSE ZONE",
		ZBC_SG_CLOSE_ZONE_CDB_OPCODE,
		ZBC_SG_CLOSE_ZONE_CDB_SA,
		ZBC_SG_CLOSE_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_FINISH_ZONE */
	{
		"FINISH ZONE",
		ZBC_SG_FINISH_ZONE_CDB_OPCODE,
		ZBC_SG_FINISH_ZONE_CDB_SA,
		ZBC_SG_FINISH_ZONE_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_RESET_WRITE_POINTER */
	{
		"RESET WRITE POINTER",
		ZBC_SG_RESET_WRITE_POINTER_CDB_OPCODE,
		ZBC_SG_RESET_WRITE_POINTER_CDB_SA,
		ZBC_SG_RESET_WRITE_POINTER_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_SET_ZONES */
	{
		"SET ZONES",
		ZBC_SG_SET_ZONES_CDB_OPCODE,
		ZBC_SG_SET_ZONES_CDB_SA,
		ZBC_SG_SET_ZONES_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_SET_WRITE_POINTER */
	{
		"SET WRITE POINTER",
		ZBC_SG_SET_WRITE_POINTER_CDB_OPCODE,
		ZBC_SG_SET_WRITE_POINTER_CDB_SA,
		ZBC_SG_SET_WRITE_POINTER_CDB_LENGTH,
		SG_DXFER_NONE
	},

	/* ZBC_SG_ATA16 */
	{
		"ATA 16",
		ZBC_SG_ATA16_CDB_OPCODE,
		0,
		ZBC_SG_ATA16_CDB_LENGTH,
		0
	}

};

/**
 * Get a command name from its operation code in a CDB.
 */
static char *zbc_sg_cmd_name(zbc_sg_cmd_t *cmd)
{

	if (cmd->code >= 0 &&
	    cmd->code < ZBC_SG_CMD_NUM)
		return zbc_sg_cmd_list[cmd->code].cdb_cmd_name;

	return "(UNKNOWN COMMAND)";
}

/**
 * Set ASC, ASCQ.
 */
static void zbc_sg_set_sense(zbc_device_t *dev, uint8_t *sense_buf)
{

	if (sense_buf == NULL) {
		dev->zbd_errno.sk = 0x00;
		dev->zbd_errno.asc_ascq = 0x0000;
		return;
	}

	if ((sense_buf[0] & 0x7F) == 0x72 ||
	    (sense_buf[0] & 0x7F) == 0x73) {
		/* store sense key, ASC/ASCQ */
		dev->zbd_errno.sk = sense_buf[1] & 0x0F;
		dev->zbd_errno.asc_ascq =
			((int)sense_buf[2] << 8) | (int)sense_buf[3];
		return;
	}

	if ((sense_buf[0] & 0x7F) == 0x70 ||
		   (sense_buf[0] & 0x7F) == 0x71) {
		/* store sense key, ASC/ASCQ */
		dev->zbd_errno.sk = sense_buf[2] & 0x0F;
		dev->zbd_errno.asc_ascq =
			((int)sense_buf[12] << 8) | (int)sense_buf[13];
	}
}

/**
 * Free a command.
 */
void zbc_sg_cmd_destroy(zbc_sg_cmd_t *cmd)
{
	/* Free the command buffer */
        if (cmd->out_buf && cmd->out_buf_needfree) {
		free(cmd->out_buf);
		cmd->out_buf = NULL;
		cmd->out_bufsz = 0;
        }
}

/**
 * Allocate and initialize a new command.
 */
int zbc_sg_cmd_init(zbc_sg_cmd_t *cmd, int cmd_code,
		    uint8_t *out_buf, size_t out_bufsz)
{

	/* Set command */
	memset(cmd, 0, sizeof(zbc_sg_cmd_t));
	cmd->code = cmd_code;
	cmd->cdb_sz = zbc_sg_cmd_list[cmd_code].cdb_length;
	zbc_assert(cmd->cdb_sz <= ZBC_SG_CDB_MAX_LENGTH);
	cmd->cdb_opcode = zbc_sg_cmd_list[cmd_code].cdb_opcode;
	cmd->cdb_sa = zbc_sg_cmd_list[cmd_code].cdb_sa;

	if (!out_buf) {

		int ret;

		/* Allocate a buffer */
		ret = posix_memalign((void **) &cmd->out_buf,
				     sysconf(_SC_PAGESIZE), out_bufsz);
		if (ret != 0) {
			zbc_error("No memory for output buffer (%zu B)\n",
				  out_bufsz);
			return -ENOMEM;
		}

		memset(cmd->out_buf, 0, out_bufsz);
		cmd->out_buf_needfree = 1;

	} else {

		/* Use spzecified buffer */
		cmd->out_buf = out_buf;

	}

	cmd->out_bufsz = out_bufsz;

	/* Setup SGIO header */
	cmd->io_hdr.interface_id    = 'S';
	cmd->io_hdr.timeout         = 20000;
	cmd->io_hdr.flags           = 0; //SG_FLAG_DIRECT_IO;

	cmd->io_hdr.cmd_len         = cmd->cdb_sz;
	cmd->io_hdr.cmdp            = &cmd->cdb[0];

	cmd->io_hdr.dxfer_direction = zbc_sg_cmd_list[cmd_code].dir;
	cmd->io_hdr.dxfer_len       = cmd->out_bufsz;
	cmd->io_hdr.dxferp          = cmd->out_buf;

	cmd->io_hdr.mx_sb_len       = ZBC_SG_SENSE_MAX_LENGTH;
	cmd->io_hdr.sbp             = cmd->sense_buf;

	return 0;
}

/**
 * Default maximum number of SG segments (128KB for 4K pages).
 */
#define ZBC_SG_MAX_SGSZ		32

/**
 * Get the maximum allowed command size of a block device.
 */
static int zbc_sg_max_segments(struct zbc_device *dev)
{
	unsigned int max_segs = ZBC_SG_MAX_SGSZ;
	FILE *fmax_segs;
	char str[128];
	int ret;

	snprintf(str, sizeof(str),
		 "/sys/block/%s/queue/max_segments",
		 basename(dev->zbd_filename));

	fmax_segs = fopen(str, "r");
	if (fmax_segs) {
		ret = fscanf(fmax_segs, "%u", &max_segs);
		if (ret != 1)
			max_segs = ZBC_SG_MAX_SGSZ;
		fclose(fmax_segs);
	}

	return max_segs;
}

/**
 * Get the maximum allowed command blocks for the device.
 */
void zbc_sg_get_max_cmd_blocks(struct zbc_device *dev)
{
	struct stat st;
	size_t sgsz = ZBC_SG_MAX_SGSZ;
	int ret;

	/* Get device stats */
	if (fstat(dev->zbd_fd, &st) < 0) {
		zbc_debug("%s: stat failed %d (%s)\n",
			  dev->zbd_filename,
			  errno, strerror(errno));
		goto out;
	}

	if (S_ISCHR(st.st_mode)) {
		ret = ioctl(dev->zbd_fd, SG_GET_SG_TABLESIZE, &sgsz);
		if (ret != 0) {
			zbc_debug("%s: SG_GET_SG_TABLESIZE ioctl failed %d (%s)\n",
				  dev->zbd_filename,
				  errno,
				  strerror(errno));
			sgsz = ZBC_SG_MAX_SGSZ;
		}
		if (!sgsz)
			sgsz = 1;
	} else if (S_ISBLK(st.st_mode)) {
		sgsz = zbc_sg_max_segments(dev);
	} else {
		/* Files for fake backend */
		sgsz = 128;
	}

out:
	dev->zbd_info.zbd_max_rw_sectors =
		(uint32_t)sgsz * sysconf(_SC_PAGESIZE) >> 9;

	zbc_debug("%s: Maximum command data transfer size is %llu sectors\n",
		  dev->zbd_filename,
		  (unsigned long long)dev->zbd_info.zbd_max_rw_sectors);
}

/**
 * Execute a command.
 */
int zbc_sg_cmd_exec(zbc_device_t *dev, zbc_sg_cmd_t *cmd)
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

	/* Send the SG_IO command */
	ret = ioctl(dev->zbd_fd, SG_IO, &cmd->io_hdr);
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
			zbc_sg_set_sense(dev, cmd->sense_buf);
			return -EIO;
		}

		if ((zbc_sg_cmd_driver_status(cmd) == ZBC_SG_DRIVER_SENSE) &&
		    (cmd->io_hdr.sb_len_wr > 21) &&
		    (cmd->sense_buf[21] != 0x50) ) {
			zbc_sg_set_sense(dev, cmd->sense_buf);
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

		zbc_sg_set_sense(dev, cmd->sense_buf);

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
 * Test if unit is ready. This will retry 5 times if the command
 * returns "UNIT ATTENTION".
 */
int zbc_sg_cmd_test_unit_ready(zbc_device_t *dev)
{
	zbc_sg_cmd_t cmd;
	int ret = -EAGAIN, retries = 5;

	while (retries && (ret == -EAGAIN)) {

		retries--;

		/* Intialize command */
		ret = zbc_sg_cmd_init(&cmd, ZBC_SG_TEST_UNIT_READY, NULL, 0);
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
 * Fill the buffer with the result of INQUIRY command.
 * buf must be at least ZBC_SG_INQUIRY_REPLY_LEN bytes long.
 */
int zbc_sg_cmd_inquiry(zbc_device_t *dev, void *buf)
{
	zbc_sg_cmd_t cmd;
	int ret;

	/* Allocate and intialize inquiry command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_INQUIRY, NULL, ZBC_SG_INQUIRY_REPLY_LEN);
	if (ret != 0) {
		zbc_error("%s: zbc_sg_cmd_init INQUIRY failed\n",
			  dev->zbd_filename);
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * | 0   |                           Operation Code (12h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   | Logical Unit Number      |                  Reserved         |  EVPD  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |                           Page Code                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   | (MSB)                                                                 |
	 * |- - -+---                    Allocation Length                            ---|
	 * | 4   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_INQUIRY_CDB_OPCODE;
	zbc_sg_cmd_set_int16(&cmd.cdb[3], ZBC_SG_INQUIRY_REPLY_LEN);

	/* Execute the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret == 0)
		memcpy(buf, cmd.out_buf, ZBC_SG_INQUIRY_REPLY_LEN);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get a device capacity information (total sectors & sector sizes).
 */
int zbc_sg_get_capacity(zbc_device_t *dev,
			int (*report_zones)(struct zbc_device *,
					    uint64_t,
					    enum zbc_reporting_options,
					    uint64_t *, zbc_zone_t *,
					    unsigned int *))
{
	zbc_sg_cmd_t cmd;
	zbc_zone_t *zones = NULL;
	int logical_per_physical;
	unsigned int nr_zones = 0;
	uint64_t max_lba;
	int ret;

	/* READ CAPACITY 16 */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_READ_CAPACITY,
			      NULL, ZBC_SG_READ_CAPACITY_REPLY_LEN);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB */
	cmd.cdb[0] = ZBC_SG_READ_CAPACITY_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_READ_CAPACITY_CDB_SA;
	zbc_sg_cmd_set_int32(&cmd.cdb[10], ZBC_SG_READ_CAPACITY_REPLY_LEN);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	/* Logical block size */
	dev->zbd_info.zbd_lblock_size = zbc_sg_cmd_get_int32(&cmd.out_buf[8]);
	if (dev->zbd_info.zbd_lblock_size == 0) {
		zbc_error("%s: invalid logical sector size\n",
			  dev->zbd_filename);
		ret = -EINVAL;
		goto out;
	}

	logical_per_physical = 1 << cmd.out_buf[13] & 0x0f;

	/* Get maximum command size */
	zbc_sg_get_max_cmd_blocks(dev);

	/* Check RC_BASIS field */
	switch ((cmd.out_buf[12] & 0x30) >> 4) {

	case 0x00:

		/*
		 * The capacity represents only the space used by
		 * conventional zones at the beginning of the disk. To get
		 * the entire device capacity, we need to get the last LBA
		 * of the last zone of the disk.
		 */
		ret = report_zones(dev, 0, ZBC_RO_ALL, &max_lba,
				   NULL, &nr_zones);
		if (ret != 0)
			goto out;

		/* Set the drive capacity to the reported max LBA */
		dev->zbd_info.zbd_lblocks = max_lba + 1;

		break;

	case 0x01:

		/* The disk last LBA was reported */
		dev->zbd_info.zbd_lblocks =
			zbc_sg_cmd_get_int64(&cmd.out_buf[0]) + 1;

		break;

	default:

		zbc_error("%s: invalid RC_BASIS field encountered "
			  "in READ CAPACITY result\n",
			  dev->zbd_filename);
		ret = -EIO;

		goto out;

	}

	if (!dev->zbd_info.zbd_lblocks) {
		zbc_error("%s: invalid capacity (logical blocks)\n",
			  dev->zbd_filename);
		ret = -EINVAL;
		goto out;
	}

	dev->zbd_info.zbd_pblock_size =
		dev->zbd_info.zbd_lblock_size * logical_per_physical;
	dev->zbd_info.zbd_pblocks =
		dev->zbd_info.zbd_lblocks / logical_per_physical;
	dev->zbd_info.zbd_sectors =
		(dev->zbd_info.zbd_lblocks * dev->zbd_info.zbd_lblock_size) >> 9;

out:
	zbc_sg_cmd_destroy(&cmd);

	if (zones)
		free(zones);

	return ret;
}

/**
 * Set bytes in a command cdb.
 */
void zbc_sg_cmd_set_bytes(uint8_t *cmd, void *buf, int bytes)
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
void zbc_sg_cmd_get_bytes(uint8_t *val, union converter *conv, int bytes)
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
void zbc_sg_print_bytes(zbc_device_t *dev, uint8_t *buf, unsigned int len)
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
		if (i < (len - 4))
			zbc_debug("%s: * |=====+======+======+======+======+\n",
				  dev->zbd_filename);
		else
			zbc_debug("%s: * +==================================\n",
				  dev->zbd_filename);
	}
}

