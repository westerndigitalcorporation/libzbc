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
#define ZBC_ATA_CAPACITY_PAGE			0x02
#define ZBC_ATA_SUPPORTED_CAPABILITIES_PAGE	0x03
#define ZBC_ATA_STRINGS_PAGE			0x05
#define ZBC_ATA_ZONED_DEVICE_INFORMATION_PAGE	0x09

/**
 * Driver device flags.
 */
enum zbc_ata_drv_flags {

	/** Use SCSI SBC commands for I/O operations */
	ZBC_ATA_USE_SBC		= 0x00000001,

};

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
static int zbc_ata_read_log(struct zbc_device *dev, uint8_t log,
			    int page, uint8_t *buf, size_t bufsz)
{
	unsigned int lba_count = bufsz / 512;
	struct zbc_sg_cmd cmd;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, buf, bufsz);
	if (ret != 0)
		return ret;

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
static int zbc_ata_set_features(struct zbc_device *dev, uint8_t feature,
				uint8_t count)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0)
		return ret;

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
 * Sense data is enabled.
 */
static inline int zbc_ata_sense_data_enabled(struct zbc_sg_cmd *cmd)
{
	/* Descriptor code and status including sense data flag */
	return cmd->io_hdr.sb_len_wr > 8 &&
		cmd->sense_buf[8] == 0x09 &&
		cmd->sense_buf[21] & 0x02;
}

/**
 * Request sense data.
 */
static void zbc_ata_request_sense_data_ext(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0) {
		zbc_error("%s: Get sense data zbc_sg_cmd_init failed\n",
			  dev->zbd_filename);
		return;
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
		zbc_error("%s: REQUEST SENSE DATA command failed\n",
			  dev->zbd_filename);
		goto out;
	}

	if (!cmd.io_hdr.sb_len_wr) {
		zbc_error("%s: No sense data\n", dev->zbd_filename);
		goto out;
	}

	if (zbc_log_level >= ZBC_LOG_DEBUG) {
		zbc_debug("%s: Sense data (%d B):\n",
			  dev->zbd_filename, cmd.io_hdr.sb_len_wr);
		zbc_sg_print_bytes(dev, cmd.sense_buf, cmd.io_hdr.sb_len_wr);
	}

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

	zbc_set_errno(cmd.sense_buf[19] & 0xF,
		      ((int)cmd.sense_buf[17] << 8) | (int)cmd.sense_buf[15]);

out:
	zbc_sg_cmd_destroy(&cmd);

	return;
}

/**
 * Get an ATA string.
 */
static int zbc_ata_strcpy(char *dst, char *buf, int buf_len,
			  int skip)
{
	int slen = 0;
	int len;

	if (skip) {
		buf_len -= skip;
		buf += skip;
	}
	len = buf_len >> 1;

	while (len) {

		if (buf[slen + 1] == 0)
			break;
		dst[slen] = buf[slen + 1];
		slen++;

		if (buf[slen - 1] == 0)
			break;
		dst[slen] = buf[slen - 1];
		slen++;

		len--;
	}

	dst[slen] = ' ';
	dst[slen + 1] = '\0';

	return slen + 1;
}

/**
 * Get device vendor, product ID and revision.
 */
static void zbc_ata_vendor_id(struct zbc_device *dev)
{
	uint8_t buf[512];
	int n, ret;

	/* Get log 30h page 05h (ATA strings) */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_STRINGS_PAGE,
			       buf,
			       sizeof(buf));
	if (ret != 0) {
		zbc_debug("%s: Get strings log page failed %d\n",
			  dev->zbd_filename, ret);
		strcpy(&dev->zbd_info.zbd_vendor_id[0], "UNKNOWN");
		return;
	}

        /* Vendor = "ATA" */
        strcpy(&dev->zbd_info.zbd_vendor_id[0], "ATA ");
	n = 4;

        /* Model number */
        n += zbc_ata_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			    (char *)&buf[48], 16, 0);

        /* Firmware revision */
        zbc_ata_strcpy(&dev->zbd_info.zbd_vendor_id[n],
		       (char *)&buf[32], 8, 4);
}

/**
 * Get zoned device information (maximum or optimal number of open zones,
 * read restriction, etc)). Data log 30h, page 09h.
 */
static int zbc_ata_get_zoned_device_info(struct zbc_device *dev)
{
	uint8_t buf[512];
	uint32_t val;
	int ret;

	if (!zbc_dev_is_zoned(dev))
		return 0;

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
	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_AWARE) {

		val = zbc_ata_get_qword(&buf[24]) & 0xffffffff;
		if (!val) {
			/* Handle this case as "not reported" */
			zbc_warning("%s: invalid optimal number of open "
				    "sequential write preferred zones\n",
				    dev->zbd_filename);
			val = ZBC_NOT_REPORTED;
		}
		dev->zbd_info.zbd_opt_nr_open_seq_pref = val;

		val = zbc_ata_get_qword(&buf[32]) & 0xffffffff;
		if (!val) {
			/* Handle this case as "not reported" */
			zbc_warning("%s: invalid optimal number of randomly "
				    "writen sequential write preferred zones\n",
				    dev->zbd_filename);
			val = ZBC_NOT_REPORTED;
		}
		dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref = val;

		dev->zbd_info.zbd_max_nr_open_seq_req = 0;

	} else {

		dev->zbd_info.zbd_opt_nr_open_seq_pref = 0;
		dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref = 0;

		val = zbc_ata_get_qword(&buf[40]) & 0xffffffff;
		if (!val) {
			/* Handle this case as "no limit" */
			zbc_warning("%s: invalid maximum number of open "
				    "sequential write required zones\n",
				    dev->zbd_filename);
			val = ZBC_NO_LIMIT;
		}
		dev->zbd_info.zbd_max_nr_open_seq_req = val;

	}

	return 0;
}

/**
 * Read from a ZAC device using READ DMA EXT packed
 * in an ATA PASSTHROUGH command.
 */
static ssize_t zbc_ata_native_pread(struct zbc_device *dev, void *buf,
				    size_t count, uint64_t offset)
{
	uint32_t lba_count = zbc_dev_sect2lba(dev, count);
	uint64_t lba_offset = zbc_dev_sect2lba(dev, offset);
	size_t sz = count << 9;
	struct zbc_sg_cmd cmd;
	ssize_t ret;

	if (dev->zbd_drv_flags & ZBC_ATA_USE_SBC)
		return zbc_scsi_pread(dev, buf, count, offset);

	/* Check */
	if (count > 65536) {
		zbc_error("%s: Read operation too large (limited to 65536 x 512 B sectors)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Initialize the command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, buf, sz);
	if (ret != 0)
		return ret;

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
		ret = (sz - cmd.io_hdr.resid) >> 9;
	}

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Read from a ZAC device.
 */
static ssize_t zbc_ata_pread(struct zbc_device *dev, void *buf,
			     size_t count, uint64_t offset)
{
	if (dev->zbd_drv_flags & ZBC_ATA_USE_SBC)
		return zbc_scsi_pread(dev, buf, count, offset);

	return zbc_ata_native_pread(dev, buf, count, offset);
}

/**
 * Write to a ZAC device using WRITE DMA EXT packed
 * in an ATA PASSTHROUGH command.
 */
static ssize_t zbc_ata_native_pwrite(struct zbc_device *dev, const void *buf,
				     size_t count, uint64_t offset)
{
	size_t sz = count << 9;
	uint32_t lba_count = zbc_dev_sect2lba(dev, count);
	uint64_t lba_offset = zbc_dev_sect2lba(dev, offset);
	struct zbc_sg_cmd cmd;
	int ret;

	/* Check */
	if (count > 65536) {
		zbc_error("%s: Write operation too large (limited to 65536 x 512 B sectors)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Initialize the command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, (uint8_t *)buf, sz);
	if (ret != 0)
		return ret;

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
		ret = (sz - cmd.io_hdr.resid) >> 9;
	}

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Write to a ZAC device.
 */
static ssize_t zbc_ata_pwrite(struct zbc_device *dev, const void *buf,
			      size_t count, uint64_t offset)
{
	if (dev->zbd_drv_flags & ZBC_ATA_USE_SBC)
		return zbc_scsi_pwrite(dev, buf, count, offset);

	return zbc_ata_native_pwrite(dev, buf, count, offset);
}

/**
 * Flush a ZAC device cache.
 */
static int zbc_ata_native_flush(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Initialize the command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0)
		return ret;

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
 * Flush a ZAC device cache.
 */
static int zbc_ata_flush(struct zbc_device *dev)
{
	if (dev->zbd_drv_flags & ZBC_ATA_USE_SBC)
		return zbc_scsi_flush(dev);

	return zbc_ata_native_flush(dev);
}

/**
 * Get device zone information.
 */
static int zbc_ata_report_zones(struct zbc_device *dev, uint64_t sector,
				enum zbc_reporting_options ro,
				struct zbc_zone *zones, unsigned int *nr_zones)
{
	size_t bufsz = ZBC_ZONE_DESCRIPTOR_OFFSET;
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int i, nz = 0, buf_nz;
	size_t max_bufsz;
	struct zbc_sg_cmd cmd;
	uint8_t *buf;
	int ret;

	if (*nr_zones)
		bufsz += (size_t)*nr_zones * ZBC_ZONE_DESCRIPTOR_LENGTH;

	bufsz = (bufsz + 4095) & ~4095;
	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

	/* Allocate and intialize report zones command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, bufsz);
	if (ret != 0)
		return ret;

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
	cmd.cdb[8]  = lba & 0xff;
	cmd.cdb[10] = (lba >>  8) & 0xff;
	cmd.cdb[12] = (lba >> 16) & 0xff;
	cmd.cdb[7]  = (lba >> 24) & 0xff;
	cmd.cdb[9]  = (lba >> 32) & 0xff;
	cmd.cdb[11] = (lba >> 40) & 0xff;
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_IN;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0) {
		/* Get sense data if enabled */
		if (ret == -EIO &&
		    zbc_ata_sense_data_enabled(&cmd) &&
		    ((zerrno.sk != ZBC_SK_ILLEGAL_REQUEST) ||
		     (zerrno.asc_ascq !=
		      ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE)))
			zbc_ata_request_sense_data_ext(dev);
		goto out;
	}

	if (cmd.out_bufsz < ZBC_ZONE_DESCRIPTOR_OFFSET ) {
		zbc_error("%s: Not enough data received (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_ZONE_DESCRIPTOR_OFFSET,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get number of zones in result */
	buf = (uint8_t *) cmd.out_buf;
	nz = zbc_ata_get_dword(buf) / ZBC_ZONE_DESCRIPTOR_LENGTH;
	/* max_lba = zbc_ata_get_qword(&buf[8]); */

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
        for (i = 0; i < nz; i++) {

		zones[i].zbz_type = buf[0] & 0x0f;

		zones[i].zbz_attributes = buf[1] & 0x03;
		zones[i].zbz_condition = (buf[1] >> 4) & 0x0f;

		zones[i].zbz_length =
			zbc_dev_lba2sect(dev, zbc_ata_get_qword(&buf[8]));
		zones[i].zbz_start =
			zbc_dev_lba2sect(dev, zbc_ata_get_qword(&buf[16]));
		if (zbc_zone_sequential(&zones[i]))
			zones[i].zbz_write_pointer =
				zbc_dev_lba2sect(dev, zbc_ata_get_qword(&buf[24]));
		else
			zones[i].zbz_write_pointer = (uint64_t)-1;

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
 * Zone(s) operation.
 */
static int zbc_ata_zone_op(struct zbc_device *dev, uint64_t sector,
			   enum zbc_zone_op op, unsigned int flags)
{
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int af;
	struct zbc_sg_cmd cmd;
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
		zbc_error("%s: Invalid operation code 0x%x\n",
			  dev->zbd_filename, op);
		return -EINVAL;
	}

	/* Intialize command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0)
		return ret;

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
		/* Operate on the zone at lba */
		cmd.cdb[8] = lba & 0xff;
		cmd.cdb[10] = (lba >> 8) & 0xff;
		cmd.cdb[12] = (lba >> 16) & 0xff;
		cmd.cdb[7] = (lba >> 24) & 0xff;
		cmd.cdb[9] = (lba >> 32) & 0xff;
		cmd.cdb[11] = (lba >> 40) & 0xff;
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
static int zbc_ata_classify(struct zbc_device *dev)
{
	uint8_t buf[512];
	uint64_t zoned;
	struct zbc_sg_cmd cmd;
	unsigned int sig;
	uint8_t *desc;
	int ret;

	/* Intialize command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, 0);
	if (ret != 0)
		return ret;

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
	if (ret != 0) {
		/* Probably not an ATA device */
		ret = -ENXIO;
		goto out;
	}

	/* It worked, so we can safely assume that this is an ATA device */
	dev->zbd_info.zbd_type = ZBC_DT_ATA;

	/* Test device signature */
	desc = &cmd.sense_buf[8];

	zbc_debug("%s: Device signature is %02x:%02x\n",
		  dev->zbd_filename, desc[9], desc[11]);

	sig = (unsigned int)desc[11] << 8 | desc[9];
	switch (sig) {

	case 0xABCD:
		/* ZAC host-managed signature */
		zbc_debug("%s: Host-managed ZAC signature detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
		break;

	case 0x0000:
		/* Standard block device */
		break;

	default:
		/* Unsupported device */
		zbc_debug("%s: Unsupported device (signature %02x:%02x)\n",
			  dev->zbd_filename, desc[9], desc[11]);
		dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
		ret = -ENXIO;
		goto out;
	}

	/*
	 * If the device has a standard block device type, the device
	 * may be a host-aware one. So look at the block device characteristics
	 * VPD page (B1h) to be sure. Also check that no weird value is
	 * reported by the zoned field for host-managed devices.
	 */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_SUPPORTED_CAPABILITIES_PAGE,
			       buf,
			       sizeof(buf));
	if (ret != 0) {
		zbc_debug("%s: Get supported capabilities page failed\n",
			  dev->zbd_filename);
		ret = -ENXIO;
		goto out;
	}

	zoned = zbc_ata_get_qword(&buf[104]);
	if (!(zoned  & (1ULL << 63)))
		zoned = 0;

	zoned = zoned & 0x03;
	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED) {
		if (zbc_test_mode(dev) && zoned != 0) {
			zbc_error("%s: Invalid host-managed device ZONED field 0x%02x\n",
				  dev->zbd_filename, (unsigned int)zoned);
			ret = -EIO;
		} else if (zoned != 0) {
			zbc_warning("%s: Invalid host-managed device ZONED field 0x%02x\n",
				    dev->zbd_filename, (unsigned int)zoned);
		}
		goto out;
	}

	switch (zoned) {

	case 0x00:
		zbc_debug("%s: Standard ATA device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
		ret = -ENXIO;
		break;

	case 0x01:
		zbc_debug("%s: Host-aware ATA device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
		break;

	case 0x02:
		zbc_debug("%s: Device-managed ATA device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_DEVICE_MANAGED;
		ret = -ENXIO;
		break;

	default:
		zbc_debug("%s: Unknown device model 0x%02x\n",
			  dev->zbd_filename, (unsigned int)zoned);
		dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
		ret = -EIO;
		break;
	}

out:
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get a device capacity information (total blocks & block size).
 */
static int zbc_ata_get_capacity(struct zbc_device *dev)
{
	uint8_t buf[512];
	uint64_t qword;
	int logical_per_physical;
	int ret;

	/* Get capacity log page */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_CAPACITY_PAGE,
			       buf,
			       sizeof(buf));
	if (ret != 0) {
		zbc_error("%s: Get supported capabilities page failed\n",
			  dev->zbd_filename);
		return ret;
	}

	/* Total capacity (logical blocks) */
	qword = zbc_ata_get_qword(&buf[8]);
	dev->zbd_info.zbd_lblocks = qword & 0x0000ffffffffffff;
	if (!(qword & (1ULL << 63)) ||
	    dev->zbd_info.zbd_lblocks == 0) {
		zbc_error("%s: invalid capacity (logical blocks)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Logical block size */
	qword = zbc_ata_get_qword(&buf[16]);
	if (!(qword & (1ULL << 63))) {
		zbc_error("%s: invalid Physical/Logical Sector Size field\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	if (qword & (1ULL << 61))
		/* LOGICAL SECTOR SIZE SUPPORTED bit set */
		dev->zbd_info.zbd_lblock_size =
			(zbc_ata_get_qword(&buf[24]) & 0xffffffff) << 1;
	else
		/* 512B */
		dev->zbd_info.zbd_lblock_size = 512;
	if (dev->zbd_info.zbd_lblock_size < 512) {
		zbc_error("%s: invalid logical sector size\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	if (qword & (1ULL << 62))
		logical_per_physical = 1 << ((qword >> 16) & 0x7);
	else
		logical_per_physical = 1;
	if (!logical_per_physical) {
		zbc_error("%s: invalid logical-per-physical value\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Get maximum command size */
	zbc_sg_get_max_cmd_blocks(dev);

	dev->zbd_info.zbd_pblock_size =
		dev->zbd_info.zbd_lblock_size * logical_per_physical;
	dev->zbd_info.zbd_pblocks =
		dev->zbd_info.zbd_lblocks / logical_per_physical;
	dev->zbd_info.zbd_sectors =
		(dev->zbd_info.zbd_lblocks * dev->zbd_info.zbd_lblock_size) >> 9;

	return 0;
}

/**
 * Test SBC SAT for regular commands (read, write, flush).
 */
static void zbc_ata_test_sbc_sat(struct zbc_device *dev)
{
	char buf[4096];
	int ret;

	ret = zbc_scsi_pread(dev, buf, 8, 0);
	if (ret == 8) {
		dev->zbd_drv_flags |= ZBC_ATA_USE_SBC;
		zbc_error("%s: Using SCSI commands for read/write/flush operations\n",
			  dev->zbd_filename);
	}
}

/**
 * Get a device information (capacity & sector sizes).
 */
static int zbc_ata_get_dev_info(struct zbc_device *dev)
{
	int ret;

	/* Make sure the device is ready */
	ret = zbc_sg_test_unit_ready(dev);
	if (ret != 0)
		return ret;

	/* Get device model */
	ret = zbc_ata_classify(dev);
	if (ret != 0)
		return ret;

	/* Get capacity information */
	ret = zbc_ata_get_capacity(dev);
	if (ret != 0 )
		return ret;

	/* Get vendor information */
	zbc_ata_vendor_id(dev);

	/* Get zoned device information */
	ret = zbc_ata_get_zoned_device_info(dev);
	if (ret != 0)
		return ret;

	/* Check if we have a functional SAT for read/write */
	if (!zbc_test_mode(dev))
		zbc_ata_test_sbc_sat(dev);

	return 0;
}

/**
 * Open a device.
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
	fd = open(filename, flags & ZBC_O_MODE_MASK);
	if (fd < 0 ) {
		ret = -errno;
		zbc_error("%s: Open device file failed %d (%s)\n",
			  filename,
			  errno,
			  strerror(errno));
		goto out;
	}

	/* Check device */
	if (fstat(fd, &st) != 0) {
		ret = -errno;
		zbc_error("%s: Stat device file failed %d (%s)\n",
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
	dev->zbd_sg_fd = fd;
#ifdef HAVE_DEVTEST
	dev->zbd_o_flags = flags & ZBC_O_DEVTEST;
#endif
	if (flags & O_DIRECT)
		dev->zbd_o_flags |= ZBC_O_DIRECT;

	dev->zbd_filename = strdup(filename);
	if (!dev->zbd_filename)
		goto out_free_dev;

	ret = zbc_ata_get_dev_info(dev);
	if (ret != 0)
		goto out_free_filename;

	/* Set sense data reporting */
	ret = zbc_ata_set_features(dev,
			ZBC_ATA_ENABLE_SENSE_DATA_REPORTING, 0x01);
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

static int zbc_ata_close(struct zbc_device *dev)
{

	if (close(dev->zbd_fd))
		return -errno;

	free(dev->zbd_filename);
	free(dev);

	return 0;
}

/**
 * ZAC ATA backend driver definition.
 */
struct zbc_drv zbc_ata_drv =
{
	.flag			= ZBC_O_DRV_ATA,
	.zbd_open		= zbc_ata_open,
	.zbd_close		= zbc_ata_close,
	.zbd_pread		= zbc_ata_pread,
	.zbd_pwrite		= zbc_ata_pwrite,
	.zbd_flush		= zbc_ata_flush,
	.zbd_report_zones	= zbc_ata_report_zones,
	.zbd_zone_op		= zbc_ata_zone_op,
};

