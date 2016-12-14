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
 */

#include "zbc.h"
#include "zbc_sg.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * Number of bytes in a Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_LENGTH		64

/**
 * Number of bytes in the buffer before the first Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_OFFSET		64

/**
 * ATA commands.
 */
#define ZBC_ATA_IDENTIFY			0xEC
#define ZBC_ATA_EXEC_DEV_DIAGNOSTIC		0x90
#define ZBC_ATA_READ_LOG_DMA_EXT		0x47
#define ZBC_ATA_SET_FEATURES			0xEF
#define ZBC_ATA_REQUEST_SENSE_DATA_EXT		0x0B
#define ZBC_ATA_READ_DMA_EXT			0x25
#define ZBC_ATA_WRITE_DMA_EXT			0x35
#define ZBC_ATA_FLUSH_CACHE_EXT			0xEA
#define ZBC_ATA_ZAC_MANAGEMENT_IN		0x4A
#define ZBC_ATA_ZAC_MANAGEMENT_OUT		0x9F
#define ZBC_ATA_ENABLE_SENSE_DATA_REPORTING	0xC3

/**
 * Zone commands
 */
#define ZBC_ATA_REPORT_ZONES_EXT_AF		0x00
#define ZBC_ATA_CLOSE_ZONE_EXT_AF		0x01
#define ZBC_ATA_FINISH_ZONE_EXT_AF		0x02
#define ZBC_ATA_OPEN_ZONE_EXT_AF		0x03
#define ZBC_ATA_RESET_WRITE_POINTER_EXT_AF	0x04

#define ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR	0x30
#define ZBC_ATA_SUPPORTED_CAPABILITIES_PAGE	0x03
#define ZBC_ATA_ZONED_DEVICE_INFORMATION_PAGE	0x09

/**
 * Get a word from a command data buffer.
 */
static inline uint16_t zbc_ata_get_word(uint8_t *buf)
{
	return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * Get a Dword from a command data buffer.
 */
static inline uint32_t zbc_ata_get_dword(uint8_t *buf)
{
	return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
		((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/**
 * Get a Qword from a command data buffer.
 */
static inline uint64_t zbc_ata_get_qword(uint8_t *buf)
{
	return (uint64_t)buf[0] | ((uint64_t)buf[1] << 8) |
		((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 24) |
		((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40) |
		((uint64_t)buf[6] << 48) | ((uint64_t)buf[7] << 56);
}

/**
 * Read a log page.
 */
static int zbc_ata_read_log(zbc_device_t *dev, uint8_t log,
			    int page, uint8_t *buf, size_t bufsz)
{
	unsigned int lba_count = bufsz / 512;
	zbc_sg_cmd_t cmd;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, buf, bufsz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                            count (15:8)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                            count (7:0)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          LBA (31:24 (15:8 if ext == 0)                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          LBA (39:32)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          LBA (47:40)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                           Device                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command                                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x6 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	cmd.cdb[5] = (lba_count >> 8) & 0xff;
	cmd.cdb[6] = lba_count & 0xff;
	cmd.cdb[8] = log;
	cmd.cdb[9] = (page >> 8) & 0xff;
	cmd.cdb[10] = page & 0xff;
	cmd.cdb[14] = ZBC_ATA_READ_LOG_DMA_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Set features
 */
static int zbc_ata_set_features(zbc_device_t *dev, uint8_t feature,
				uint8_t count)
{
	zbc_sg_cmd_t cmd;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          n/a                                          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                          n/a                                          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                          count (7:0)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          n/a                                          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          n/a                                          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          n/a                                          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |            DEVICE (7:4)           |          LBA(27:24)               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                          Command                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                          Control                                      |
	 * +=============================================================================+
	 */

	/*
	 * Note: According to SAT-3r07, the protocol should be 0x8.
	 * But if it is used, the SG/SCSI driver returns an error.
	 * So use non-data protocol... Also note that to get the
	 * device signature, the "check condition" bit must be set.
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_NONE;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* Non-Data protocol */
	cmd.cdb[1] = 0x3 << 1;
	cmd.cdb[4] = feature;
	cmd.cdb[6] = count;
	cmd.cdb[14] = ZBC_ATA_SET_FEATURES;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Set features.
 */
static int zbc_ata_enable_sense_data(zbc_device_t *dev)
{
	return zbc_ata_set_features(dev,
				    ZBC_ATA_ENABLE_SENSE_DATA_REPORTING, 1);
}

/**
 * Sense data is enabled.
 */
static inline int zbc_ata_sense_data_enabled(zbc_sg_cmd_t *cmd)
{
	/* Descriptor code and status including sense data flag */
	return cmd->io_hdr.sb_len_wr > 8 &&
		cmd->sense_buf[8] == 0x09 &&
		cmd->sense_buf[21] & 0x02;
}

/**
 * Request sense data.
 */
static int zbc_ata_request_sense_data_ext(zbc_device_t *dev)
{
	zbc_sg_cmd_t cmd;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                            count (15:8)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                            count (7:0)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          LBA (31:24 15:8)                             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          LBA (39:32)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          LBA (47:40)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                           Device                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command                                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */

	/*
	 * Note: According to SAT-3r07, the protocol should be 0x8.
	 * But if it is used, the SG/SCSI driver returns an error.
	 * So use non-data protocol... Also note that to get the
	 * device signature, the "check condition" bit must be set.
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_NONE;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* Non-Data protocol, ext=1 */
	cmd.cdb[1] = (0x3 << 1) | 0x01;
	/* off_line=0, ck_cond=1, t_type=0, t_dir=0, byt_blk=0, t_length=00 */
	cmd.cdb[2] = 0x1 << 5;
	cmd.cdb[14] = ZBC_ATA_REQUEST_SENSE_DATA_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0) {
		zbc_error("REQUEST SENSE DATA command failed\n");
		goto out;
	}

	if (!cmd.io_hdr.sb_len_wr) {
		zbc_debug("%s: No sense data\n", dev->zbd_filename);
		goto out;
	}

	zbc_debug("%s: Sense data (%d B):\n",
		  dev->zbd_filename, cmd.io_hdr.sb_len_wr);
	zbc_sg_print_bytes(dev, cmd.sense_buf, cmd.io_hdr.sb_len_wr);

	if (cmd.io_hdr.sb_len_wr <= 8) {
		zbc_debug("%s: Sense buffer length is %d (less than 8B)\n",
			  dev->zbd_filename,
			  cmd.io_hdr.sb_len_wr);
		goto out;
	}

	zbc_debug("%s: Sense key is 0x%x\n",
		  dev->zbd_filename, cmd.sense_buf[19] & 0xF);
	zbc_debug("%s: Additional sense code is 0x%02x\n",
		  dev->zbd_filename, cmd.sense_buf[17]);
	zbc_debug("%s: Additional sense code qualifier is 0x%02x\n",
		  dev->zbd_filename, cmd.sense_buf[15]);
	dev->zbd_errno.sk = cmd.sense_buf[19] & 0xF;
	dev->zbd_errno.asc_ascq =
			((int)cmd.sense_buf[17] << 8) | (int)cmd.sense_buf[15];

out:
	zbc_sg_cmd_destroy(&cmd);

	return ret;

}

/**
 * Get disk vendor, product ID and revision.
 */
static void zbc_ata_vendor_id(zbc_device_t *dev)
{
	uint8_t buf[512];
	int n, ret;

	/* Use inquiry. We could use log 30h page 05h (ATA strings) here... */
	ret = zbc_sg_cmd_inquiry(dev, buf);
	if (ret != 0) {
		zbc_debug("Device inquiry failed %d\n", ret);
		strcpy(&dev->zbd_info.zbd_vendor_id[0], "UNKNOWN");
		return;
	}

        /* Vendor identification */
        n = zbc_sg_cmd_strcpy(&dev->zbd_info.zbd_vendor_id[0],
			      (char *)&buf[8], 8);

        /* Product identification */
        n += zbc_sg_cmd_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			       (char *)&buf[16], 16);

        /* Product revision */
        zbc_sg_cmd_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			  (char *)&buf[32], 4);
}

/**
 * Get zoned device information (maximum or optimal number of open zones,
 * read restriction, etc)). Data log 30h, page 09h.
 */
static int zbc_ata_get_zoned_device_info(zbc_device_t *dev)
{
	uint8_t buf[512];
	int ret;

	/* Get zoned block device information */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_ZONED_DEVICE_INFORMATION_PAGE,
			       buf,
			       sizeof(buf));
	if (ret < 0)
		return ret;

	/* URSWRZ (unrestricted read write sequential required zone) flag */
	dev->zbd_info.zbd_flags |= (zbc_ata_get_qword(&buf[8]) & 0x01) ?
		ZBC_UNRESTRICTED_READ : 0;

	/* Maximum number of zones for resource management */
	dev->zbd_info.zbd_opt_nr_open_seq_pref =
		zbc_ata_get_qword(&buf[24]) & 0xffffffff;
	dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref =
		zbc_ata_get_qword(&buf[32]) & 0xffffffff;
	dev->zbd_info.zbd_max_nr_open_seq_req =
		zbc_ata_get_qword(&buf[40]) & 0xffffffff;

	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED &&
	    dev->zbd_info.zbd_max_nr_open_seq_req <= 0) {
		zbc_error("%s: invalid maximum number of open sequential "
			  "write required zones for host-managed device\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	return 0;
}

/**
 * Read from a ZAC device using READ DMA EXT packed
 * in an ATA PASSTHROUGH command.
 */
static ssize_t zbc_ata_pread(zbc_device_t *dev, void *buf,
			     size_t lba_count, uint64_t lba_offset)
{
	size_t sz = lba_count * dev->zbd_info.zbd_lblock_size;
	zbc_sg_cmd_t cmd;
	ssize_t ret;

	/* Check */
	if (sz > (65536 << 9)) {
		zbc_error("Read operation too large (limited to 65536 x 512 B sectors)\n");
		return -EINVAL;
	}

	/* Initialize the command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, buf, sz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                            count (15:8)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                            count (7:0)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          LBA (31:24 (15:8 if ext == 0)                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          LBA (39:32)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          LBA (47:40)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                           Device                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command                                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x6 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	cmd.cdb[5] = (lba_count >> 8) & 0xff;
	cmd.cdb[6] = lba_count & 0xff;
	cmd.cdb[7] = (lba_offset >> 24) & 0xff;
	cmd.cdb[8] = lba_offset & 0xff;
	cmd.cdb[9] = (lba_offset >> 32) & 0xff;
	cmd.cdb[10] = (lba_offset >> 8) & 0xff;
	cmd.cdb[11] = (lba_offset >> 40) & 0xff;
	cmd.cdb[12] = (lba_offset >> 16) & 0xff;
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_READ_DMA_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0) {
		/* Request sense data */
		if (ret == -EIO && zbc_ata_sense_data_enabled(&cmd) )
			zbc_ata_request_sense_data_ext(dev);
	} else {
		ret = (sz - cmd.io_hdr.resid) /
			dev->zbd_info.zbd_lblock_size;
	}

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Write to a ZAC device using WRITE DMA EXT packed
 * in an ATA PASSTHROUGH command.
 */
static ssize_t zbc_ata_pwrite(zbc_device_t *dev, const void *buf,
			      size_t lba_count, uint64_t lba_offset)
{
	size_t sz = lba_count * dev->zbd_info.zbd_lblock_size;
	zbc_sg_cmd_t cmd;
	int ret;

	/* Check */
	if ( sz > (65536 << 9)) {
		zbc_error("Write operation too large "
			  "(limited to 65536 x 512 B sectors)\n");
		return -EINVAL;
	}

	/* Initialize the command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, (uint8_t *)buf, sz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                           count (15:8)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                           count (7:0)                                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                           LBA (31:24)                                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                            LBA (7:0)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                           LBA (39:32)                                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                           LBA (15:8)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                           LBA (47:40)                                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                           LBA (23:16)                                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                             Device                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                             Command                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                             Control                                   |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x6 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=1, t_dir=0, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x06;
	cmd.cdb[5] = (lba_count >> 8) & 0xff;
	cmd.cdb[6] = lba_count & 0xff;
	cmd.cdb[7] = (lba_offset >> 24) & 0xff;
	cmd.cdb[8] = lba_offset & 0xff;
	cmd.cdb[9] = (lba_offset >> 32) & 0xff;
	cmd.cdb[10] = (lba_offset >> 8) & 0xff;
	cmd.cdb[11] = (lba_offset >> 40) & 0xff;
	cmd.cdb[12] = (lba_offset >> 16) & 0xff;
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_WRITE_DMA_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0) {
		/* Request sense data */
		if (ret == -EIO && zbc_ata_sense_data_enabled(&cmd))
			zbc_ata_request_sense_data_ext(dev);
        } else {
		ret = (sz - cmd.io_hdr.resid) / dev->zbd_info.zbd_lblock_size;
	}

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Flush a ZAC device cache.
 */
static int zbc_ata_flush(zbc_device_t *dev,
			 uint64_t lba_ofst, size_t lba_count,
			 int immediate)
{
	zbc_sg_cmd_t cmd;
	int ret;

	/* Initialize the command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB */
	cmd.io_hdr.dxfer_direction = SG_DXFER_NONE;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* Non-Data protocol, ext=1 */
	cmd.cdb[1] = (0x3 << 1) | 0x01;
	cmd.cdb[14] = ZBC_ATA_FLUSH_CACHE_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get device zone information.
 */
static int zbc_ata_do_report_zones(zbc_device_t *dev, uint64_t start_lba,
				   enum zbc_reporting_options ro,
				   uint64_t *max_lba,
				   zbc_zone_t *zones, unsigned int *nr_zones)
{
	size_t bufsz = ZBC_ZONE_DESCRIPTOR_OFFSET;
	unsigned int i, nz = 0, buf_nz;
	size_t max_bufsz;
	zbc_sg_cmd_t cmd;
	uint8_t *buf;
	int ret;

	if (*nr_zones)
		bufsz += (size_t)*nr_zones * ZBC_ZONE_DESCRIPTOR_LENGTH;

	bufsz = (bufsz + 4095) & ~4095;
	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

	/* Allocate and intialize report zones command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, NULL, bufsz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                            count (15:8)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                            count (7:0)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          LBA (31:24 (15:8 if ext == 0)                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          LBA (39:32)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          LBA (47:40)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                           Device                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command                                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x06 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	/* Partial bit and reporting options */
	cmd.cdb[3] = ro & 0xbf;
	cmd.cdb[4] = ZBC_ATA_REPORT_ZONES_EXT_AF;
	cmd.cdb[5] = ((bufsz / 512) >> 8) & 0xff;
	cmd.cdb[6] = (bufsz / 512) & 0xff;
	cmd.cdb[8]  = start_lba & 0xff;
	cmd.cdb[10] = (start_lba >>  8) & 0xff;
	cmd.cdb[12] = (start_lba >> 16) & 0xff;
	cmd.cdb[7]  = (start_lba >> 24) & 0xff;
	cmd.cdb[9]  = (start_lba >> 32) & 0xff;
	cmd.cdb[11] = (start_lba >> 40) & 0xff;
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_IN;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0) {
		/* Get sense data if enabled */
		if (ret == -EIO &&
		    zbc_ata_sense_data_enabled(&cmd) &&
		    ((dev->zbd_errno.sk != ZBC_SK_ILLEGAL_REQUEST) ||
		     (dev->zbd_errno.asc_ascq !=
		      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE)))
			zbc_ata_request_sense_data_ext(dev);
		goto out;
	}

	if (cmd.out_bufsz < ZBC_ZONE_DESCRIPTOR_OFFSET ) {
		zbc_error("Not enough data received (need at least %d B, got %zu B)\n",
			  ZBC_ZONE_DESCRIPTOR_OFFSET,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get number of zones in result */
	buf = (uint8_t *) cmd.out_buf;
	nz = zbc_ata_get_dword(buf) / ZBC_ZONE_DESCRIPTOR_LENGTH;
	if (max_lba)
		*max_lba = zbc_ata_get_qword(&buf[8]);

	if (!zones || !nz)
		goto out;

        /* Get zone info */
        if (nz > *nr_zones)
		nz = *nr_zones;

	buf_nz = (cmd.out_bufsz - ZBC_ZONE_DESCRIPTOR_OFFSET)
		/ ZBC_ZONE_DESCRIPTOR_LENGTH;
        if (nz > buf_nz)
		nz = buf_nz;

        /* Get zone descriptors */
	buf += ZBC_ZONE_DESCRIPTOR_OFFSET;
        for(i = 0; i < nz; i++) {
		zones[i].zbz_type = buf[0] & 0x0f;
		zones[i].zbz_condition = (buf[1] >> 4) & 0x0f;
		zones[i].zbz_length = zbc_ata_get_qword(&buf[8]);
		zones[i].zbz_start = zbc_ata_get_qword(&buf[16]);
		zones[i].zbz_write_pointer = zbc_ata_get_qword(&buf[24]);
		zones[i].zbz_attributes = buf[1] & 0x03;
		buf += ZBC_ZONE_DESCRIPTOR_LENGTH;
        }

out:
	/* Return number of zones */
	*nr_zones = nz;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get device zone information.
 */
static int zbc_ata_report_zones(zbc_device_t *dev, uint64_t start_lba,
				enum zbc_reporting_options ro,
				zbc_zone_t *zones, unsigned int *nr_zones)
{
	return zbc_ata_do_report_zones(dev, start_lba, ro,
				       NULL, zones, nr_zones);
}

/**
 * Zone(s) operation.
 */
static int zbc_ata_zone_op(zbc_device_t *dev, uint64_t start_lba,
			   enum zbc_zone_op op, unsigned int flags)
{
	unsigned int af;
	zbc_sg_cmd_t cmd;
	int ret;

	switch (op) {
	case ZBC_OP_OPEN_ZONE:
		af = ZBC_ATA_OPEN_ZONE_EXT_AF;
		break;
	case ZBC_OP_CLOSE_ZONE:
		af = ZBC_ATA_CLOSE_ZONE_EXT_AF;
		break;
	case ZBC_OP_FINISH_ZONE:
		af = ZBC_ATA_FINISH_ZONE_EXT_AF;
		break;
	case ZBC_OP_RESET_ZONE:
		af = ZBC_ATA_RESET_WRITE_POINTER_EXT_AF;
		break;
	default:
		return -EINVAL;
	}

	/* Intialize command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                            count (15:8)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                            count (7:0)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          LBA (31:24 (15:8 if ext == 0)                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          LBA (39:32)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          LBA (47:40)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                           Device                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command                                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_NONE;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* Non-Data protocol, ext=1 */
	cmd.cdb[1] = (0x3 << 1) | 0x01;
	cmd.cdb[4] = af;

	if (flags & ZBC_OP_ALL_ZONES) {
		/* Operate on all zones */
		cmd.cdb[3] = 0x01;
	} else {
		/* Operate on the zone at start_lba */
		cmd.cdb[8] = start_lba & 0xff;
		cmd.cdb[10] = (start_lba >> 8) & 0xff;
		cmd.cdb[12] = (start_lba >> 16) & 0xff;
		cmd.cdb[7] = (start_lba >> 24) & 0xff;
		cmd.cdb[9] = (start_lba >> 32) & 0xff;
		cmd.cdb[11] = (start_lba >> 40) & 0xff;
	}
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_OUT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Request sense data */
	if (ret == -EIO && zbc_ata_sense_data_enabled(&cmd))
		zbc_ata_request_sense_data_ext(dev);

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Test device signature (return device model detected).
 */
static int zbc_ata_classify(zbc_device_t *dev)
{
	uint8_t buf[512];
	uint64_t zoned;
	zbc_sg_cmd_t cmd;
	unsigned int sig;
	uint8_t *desc;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (85h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Multiple count      |              Protocol             |  ext   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |    off_line     |ck_cond | t_type | t_dir  |byt_blk |    t_length     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                          features (15:8)                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                          features (7:0)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                            count (15:8)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   |                            count (7:0)                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                          LBA (31:24 15:8)                             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                          LBA (7:0)                                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   |                          LBA (39:32)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |                          LBA (15:8)                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                          LBA (47:40)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  |                          LBA (23:16)                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                           Device                                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command                                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */

	/*
	 * Note: According to SAT-3r07, the protocol should be 0x8.
	 * But if it is used, the SG/SCSI driver returns an error.
	 * So use non-data protocol... Also note that to get the
	 * device signature, the "check condition" bit must be set.
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_NONE;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* Non-Data protocol, ext=1 */
	cmd.cdb[1] = (0x3 << 1) | 0x1;
	/* off_line=0, ck_cond=1, t_type=0, t_dir=0, byt_blk=0, t_length=00 */
	cmd.cdb[2] = 0x1 << 5;
	cmd.cdb[14] = ZBC_ATA_EXEC_DEV_DIAGNOSTIC;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	/* It worked, so we can safely assume that this is an ATA device */
	dev->zbd_info.zbd_type = ZBC_DT_ATA;

	/* Test device signature */
	desc = &cmd.sense_buf[8];

	zbc_debug("Device signature is %02x:%02x\n",
		  desc[9], desc[11]);

	sig = (unsigned int)desc[11] << 8 | desc[9];
	switch (sig) {

	case 0xABCD:
		/* ZAC host-managed signature */
		zbc_debug("Host-managed ZAC signature detected\n");
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
		break;

	case 0x0000:

		/*
		 * Normal device signature: this may be a host-aware device.
		 * So check the zoned field in the supported capabilities.
		 */
		ret = zbc_ata_read_log(dev,
				       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
				       ZBC_ATA_SUPPORTED_CAPABILITIES_PAGE,
				       buf,
				       sizeof(buf));
		if (ret != 0) {
			zbc_error("Get supported capabilities page failed\n");
			goto out;
		}

		zoned = zbc_ata_get_qword(&buf[104]);
		if (!(zoned  & (1ULL << 63)))
			zoned = 0;

		switch (zoned & 0x03) {

		case 0x00:
			zbc_debug("Standard ATA device detected\n");
			dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
			ret = -ENXIO;
			goto out;
		case 0x01:
			zbc_debug("Host-aware ATA device detected\n");
			dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
			ret = 0;
			goto out;
		case 0x02:
			zbc_debug("Device-managed ATA device detected\n");
			dev->zbd_info.zbd_model = ZBC_DM_DEVICE_MANAGED;
			ret = -ENXIO;
			goto out;
		default:
			break;
		}

		/* Fall through (unknown disk) */

	default:

		/* Unsupported device */
		zbc_debug("Unsupported device (signature %02x:%02x)\n",
			  desc[9], desc[11]);
		dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
		ret = -ENXIO;
		break;
	}

out:
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get a device information (capacity & sector sizes).
 */
static int zbc_ata_get_dev_info(zbc_device_t *dev)
{
	int ret;

	/* Make sure the device is ready */
	ret = zbc_sg_cmd_test_unit_ready(dev);
	if (ret != 0)
		return ret;

	/* Get device model */
	ret = zbc_ata_classify(dev);
	if (ret != 0)
		return ret;

	/* Get capacity information */
	ret = zbc_sg_get_capacity(dev, zbc_ata_do_report_zones);
	if (ret != 0 )
		return ret;

	/* Get vendor information */
	zbc_ata_vendor_id(dev);

	/* Get zoned device information */
	ret = zbc_ata_get_zoned_device_info(dev);
	if (ret != 0)
		return ret;

	return 0;
}

/**
 * Open a disk.
 */
static int zbc_ata_open(const char *filename,
			int flags, struct zbc_device **pdev)
{
	struct zbc_device *dev;
	struct stat st;
	int fd, ret;

	zbc_debug("%s: ########## Trying ATA driver ##########\n",
		  filename);

	/* Open the device file */
	fd = open(filename, flags);
	if (fd < 0 ) {
		ret = -errno;
		zbc_error("Open device file %s failed %d (%s)\n",
			  filename,
			  errno,
			  strerror(errno));
		goto out;
	}

	/* Check device */
	if (fstat(fd, &st) != 0) {
		ret = -errno;
		zbc_error("Stat device %s failed %d (%s)\n",
			  filename,
			  errno,
			  strerror(errno));
		goto out;
	}

	if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) {
		ret = -ENXIO;
		goto out;
	}

	/* Set device decriptor */
	ret = -ENOMEM;
	dev = calloc(1, sizeof(struct zbc_device));
	if (!dev)
		goto out;

	dev->zbd_fd = fd;
	dev->zbd_filename = strdup(filename);
	if (!dev->zbd_filename)
		goto out_free_dev;

	ret = zbc_ata_get_dev_info(dev);
	if (ret != 0)
		goto out_free_filename;

	/* Set sense data reporting */
	ret = zbc_ata_enable_sense_data(dev);
	if (ret != 0) {
		zbc_error("%s: Enable sense data reporting failed\n",
			  filename);
		goto out_free_filename;
	}

	*pdev = dev;

	zbc_debug("%s: ########## ATA driver succeeded ##########\n",
		  filename);

	return 0;

out_free_filename:
	free(dev->zbd_filename);

out_free_dev:
	free(dev);

out:
	if (fd >= 0)
		close(fd);

	zbc_debug("%s: ########## ATA driver failed %d ##########\n",
		  filename, ret);

	return ret;
}

static int zbc_ata_close(zbc_device_t *dev)
{

	if (close(dev->zbd_fd))
		return -errno;

	free(dev->zbd_filename);
	free(dev);

	return 0;
}

/**
 * ZAC with ATA HDIO operations.
 */
zbc_ops_t zbc_ata_ops =
{
	.zbd_open		= zbc_ata_open,
	.zbd_close		= zbc_ata_close,
	.zbd_pread		= zbc_ata_pread,
	.zbd_pwrite		= zbc_ata_pwrite,
	.zbd_flush		= zbc_ata_flush,
	.zbd_report_zones	= zbc_ata_report_zones,
	.zbd_zone_op		= zbc_ata_zone_op,
};

