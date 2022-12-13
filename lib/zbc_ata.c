// SPDX-License-Identifier: BSD-2-Clause
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
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
 * REPORT ZONE DOMAINS output header size.
 */
#define ZBC_RPT_DOMAINS_HEADER_SIZE		64

/**
 * REPORT ZONE DOMAINS output descriptor size.
 */
#define ZBC_RPT_DOMAINS_RECORD_SIZE		96

/*
 * REPORT REALMS output header size.
 */
#define ZBC_RPT_REALMS_HEADER_SIZE		64

/*
 * REPORT REALMS output descriptor size.
 */
#define ZBC_RPT_REALMS_RECORD_SIZE		128

/*
 * REPORT REALMS descriptor header size
 * aka the offset to the first start/end
 * descriptor.
 */
#define ZBC_RPT_REALMS_DESC_OFFSET		16

/*
 * REPORT REALMS start/end descriptor size
 */
#define ZBC_RPT_REALMS_SE_DESC_SIZE		16

/**
 * Zone activation results header size.
 */
#define ZBC_ACTV_RES_HEADER_SIZE		64

/**
 * Zone activation results record size.
 */
#define ZBC_ACTV_RES_RECORD_SIZE		32

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

/**
 * ATA SET FEATURES subcommands.
 */
#define ZBC_ATA_ENABLE_SENSE_DATA_REPORTING	0xC3

#define ZBC_ATA_ZONE_ACTIVATION_CONTROL		0x46
#define ZBC_ATA_UPDATE_URSWRZ			0x47
#define ZBC_ATA_UPDATE_MAX_ACTIVATION		0x48 /* FIXME this value is ad-hoc */

/**
 * Zone commands
 */
#define ZBC_ATA_REPORT_ZONES_EXT_AF		0x00
#define ZBC_ATA_REPORT_REALMS_AF		0x06
#define ZBC_ATA_REPORT_ZONE_DOMAINS_AF		0x07
#define ZBC_ATA_ZONE_ACTIVATE_AF		0x08
#define ZBC_ATA_ZONE_QUERY_AF			0x09

#define ZBC_ATA_CLOSE_ZONE_EXT_AF		0x01
#define ZBC_ATA_FINISH_ZONE_EXT_AF		0x02
#define ZBC_ATA_OPEN_ZONE_EXT_AF		0x03
#define ZBC_ATA_RESET_WRITE_POINTER_EXT_AF	0x04

#define ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR	0x30
#define ZBC_ATA_CAPACITY_PAGE			0x02
#define ZBC_ATA_SUPPORTED_CAPABILITIES_PAGE	0x03
#define ZBC_ATA_CURRENT_SETTINGS_PAGE		0x04
#define ZBC_ATA_STRINGS_PAGE			0x05
#define ZBC_ATA_ZONED_DEVICE_INFORMATION_PAGE	0x09

/**
 * Driver device flags.
 */
enum zbc_ata_drv_flags {

	/** Use SCSI SBC commands for I/O operations */
	ZBC_ATA_USE_SBC		= 0x00000001,

};

char *zbc_ata_cmd_name(struct zbc_sg_cmd *cmd)
{
	switch (cmd->cdb[14]) {
	case ZBC_ATA_IDENTIFY:
		return "/IDENTIFY";
	case ZBC_ATA_EXEC_DEV_DIAGNOSTIC:
		return "/EXEC DEV DIAGNOSTIC";
	case ZBC_ATA_READ_LOG_DMA_EXT:
		return "/READ LOG DMA EXT";
	case ZBC_ATA_SET_FEATURES:
		return "/SET FEATURES";
	case ZBC_ATA_REQUEST_SENSE_DATA_EXT:
		return "/REQUEST SENSE DATA EXT";
	case ZBC_ATA_READ_DMA_EXT:
		return "/READ DMA EXT";
	case ZBC_ATA_WRITE_DMA_EXT:
		return "/WRITE DMA EXT";
	case ZBC_ATA_FLUSH_CACHE_EXT:
		return "/FLUSH CACHE EXT";
	case ZBC_ATA_ENABLE_SENSE_DATA_REPORTING:
		return "/ENABLE SENSE DATA REPORTING";
	case ZBC_ATA_ZAC_MANAGEMENT_IN:
		switch (cmd->cdb[4] & 0x7f) {
		case ZBC_ATA_REPORT_ZONES_EXT_AF:
			return "/REPORT ZONES EXT";
		case ZBC_ATA_REPORT_ZONE_DOMAINS_AF:
			return "/REPORT ZONE DOMAINS EXT";
		case ZBC_ATA_REPORT_REALMS_AF:
			return "/REPORT REALMS EXT";
		case ZBC_ATA_ZONE_ACTIVATE_AF:
			return "/ZONE ACTIVATE EXT";
		case ZBC_ATA_ZONE_QUERY_AF:
			return "/ZONE QUERY EXT";
		}
		break;
	case ZBC_ATA_ZAC_MANAGEMENT_OUT:
		switch (cmd->cdb[4] & 0x7f) {
		case ZBC_ATA_CLOSE_ZONE_EXT_AF:
			return "/CLOSE ZONE EXT";
		case ZBC_ATA_FINISH_ZONE_EXT_AF:
			return "/FINISH ZONE EXT";
		case ZBC_ATA_OPEN_ZONE_EXT_AF:
			return "/OPEN ZONE EXT";
		case ZBC_ATA_RESET_WRITE_POINTER_EXT_AF:
			return "/RESET WRITE POINTER EXT";
		}
		break;
	}

	return "/UNKNOWN COMMAND";
}

/**
 * Get a word from a command data buffer.
 */
static inline uint16_t zbc_ata_get_word(uint8_t const *buf)
{
	return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * Get a Dword from a command data buffer.
 */
static inline uint32_t zbc_ata_get_dword(uint8_t const *buf)
{
	return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
		((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/**
 * Get a Qword from a command data buffer.
 */
static inline uint64_t zbc_ata_get_qword(uint8_t const *buf)
{
	return (uint64_t)buf[0] | ((uint64_t)buf[1] << 8) |
		((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 24) |
		((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40) |
		((uint64_t)buf[6] << 48) | ((uint64_t)buf[7] << 56);
}

/*
 * Decode a 48-bit ATA LBA from a QWORD.
 */
static inline uint64_t zbc_ata_get_lba(uint8_t const *buf)
{
	return (uint64_t)buf[0] | ((uint64_t)buf[1] << 8) |
	       ((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 24) |
	       ((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40);
}

/*
 * Encode an ATA LBA.
 */
static inline void zbc_ata_set_lba(uint8_t *buf, uint64_t lba)
{
	buf[1] = lba & 0xff;
	buf[3] = (lba >> 8) & 0xff;
	buf[5] = (lba >> 16) & 0xff;
	buf[0] = (lba >> 24) & 0xff;
	buf[2] = (lba >> 32) & 0xff;
	buf[4] = (lba >> 40) & 0xff;
}

/*
 * Put the ATA-encoded LBA to a CDB.
 */
static inline void zbc_ata_put_lba(uint8_t *cdb, uint64_t lba)
{
	zbc_ata_set_lba(&cdb[7], lba);
}

/*
 * Encode a SET FEATURES LBA.
 */
static inline void zbc_ata_set_feat_lba(uint8_t *buf, uint64_t lba)
{
	buf[0] = lba & 0xff;
	buf[2] = (lba >> 8) & 0xff;
	buf[4] = (lba >> 16) & 0xff;
	buf[5] = (lba >> 24) & 0xff;
	buf[5] |= 1 << 6;
}

/*
 * Put the ATA-encoded LBA to SET FEATURES CDB.
 */
static inline void zbc_ata_put_feat_lba(uint8_t *cdb, uint64_t lba)
{
	zbc_ata_set_feat_lba(&cdb[8], lba);
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

	/* Initialize command */
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
				uint64_t lba, uint8_t count)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Initialize command */
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
	zbc_ata_put_feat_lba(cmd.cdb, lba);
	cmd.cdb[14] = ZBC_ATA_SET_FEATURES;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Test if sense data is needed and can be obtained.
 */
static inline bool zbc_ata_need_sense_data(struct zbc_sg_cmd *cmd)
{
	/* The HBA may already have got sense codes */
	if (zerrno.asc_ascq)
		return false;

	return cmd->io_hdr.sb_len_wr > 8 &&
		cmd->sense_buf[8] == 0x09 && /* ATA descriptor header present */
		cmd->sense_buf[21] & 0x02;   /* ATA status error bit set */
}

/**
 * Request sense data from drive.
 */
static void zbc_ata_request_sense_data_ext(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int sk, asc, ascq;
	int ret;

	/* Initialize command */
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

	sk = cmd.sense_buf[19] & 0xF;
	asc = cmd.sense_buf[17];
	ascq = cmd.sense_buf[15];

	zbc_debug("%s: Sense key is 0x%x\n",
		  dev->zbd_filename, sk);
	zbc_debug("%s: Additional sense code is 0x%02x\n",
		  dev->zbd_filename, asc);
	zbc_debug("%s: Additional sense code qualifier is 0x%02x\n",
		  dev->zbd_filename, ascq);

	zbc_set_errno(sk, (asc << 8) | ascq);

out:
	zbc_sg_cmd_destroy(&cmd);
}

/**
 * Request sense data.
 */
static void zbc_ata_get_sense_data(struct zbc_device *dev,
				   struct zbc_sg_cmd *cmd, int ret)
{

	if (ret != -EIO || !zbc_ata_need_sense_data(cmd))
		return;

	zbc_ata_request_sense_data_ext(dev);
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
 * Report device zone domain configuration.
 */
static int zbc_ata_report_domains(struct zbc_device *dev, uint64_t sector,
				  enum zbc_domain_report_options ro,
				  struct zbc_zone_domain *domains,
				  unsigned int nr_domains)
{
	uint64_t lba = 0LL;
	size_t bufsz = ZBC_RPT_DOMAINS_HEADER_SIZE;
	unsigned int i, nd = 0, sz;
	struct zbc_sg_cmd cmd;
	uint8_t const *buf;
	int ret;

	if (domains)
		bufsz += (size_t)nr_domains * ZBC_RPT_DOMAINS_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);
	if (sector)
		lba = zbc_dev_sect2lba(dev, sector);

	/* Allocate and initialize REPORT ZONE DOMAINS command */
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
	 * | 3   |                  features (15:8) Reporting options                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                    features (7:0), action (07h)                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5-6 |                               count                                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7-12|                           LBA (ZONE ID)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                   Device, bit 6 shall be set to 1                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command (4Ah)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                             Control                                   |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x06 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	/* Fill RO, LBA, AF, Count and Device */
	cmd.cdb[3] = ro & 0x3f;
	cmd.cdb[4] = ZBC_ATA_REPORT_ZONE_DOMAINS_AF;
	cmd.cdb[5] = ((bufsz / 512) >> 8) & 0xff;
	cmd.cdb[6] = (bufsz / 512) & 0xff;
	zbc_ata_put_lba(cmd.cdb, lba);
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_IN;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret) {
		struct zbc_err_ext zbc_err;

		/* Get sense data if enabled */
		zbc_ata_get_sense_data(dev, &cmd, ret);

		zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
		zbc_debug("REPORT ZONE DOMAINS command failed, %s/%s\n",
			  zbc_sk_str(zbc_err.sk),
			  zbc_asc_ascq_str(zbc_err.asc_ascq));
		goto out;
	}

	if (cmd.bufsz < ZBC_RPT_DOMAINS_HEADER_SIZE) {
		zbc_error("%s: Not enough REPORT ZONE DOMAINS data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_RPT_DOMAINS_HEADER_SIZE,
			  cmd.bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get the number of domain descriptors in result */
	buf = cmd.buf;
	nd = buf[9];

	if (!domains || !nd)
		goto out;

	/* Get the number of domain descriptors to fill */
	if (nd < nr_domains)
		nr_domains = nd;

	sz = zbc_ata_get_dword(&buf[4]);
	bufsz = sz < cmd.bufsz ? sz : cmd.bufsz;
	bufsz -= ZBC_RPT_DOMAINS_HEADER_SIZE;
	bufsz /= ZBC_RPT_DOMAINS_RECORD_SIZE;
	if (nr_domains > bufsz)
		nr_domains = bufsz;

	/* Get zone domain descriptors */
	buf += ZBC_RPT_DOMAINS_HEADER_SIZE;
	for (i = 0; i < nr_domains; i++) {
		domains[i].zbm_id = buf[0];

		domains[i].zbm_nr_zones = zbc_ata_get_qword(&buf[16]);
		domains[i].zbm_start_sector =
			zbc_dev_lba2sect(dev, zbc_ata_get_qword(&buf[24]));
		domains[i].zbm_end_sector =
			zbc_dev_lba2sect(dev, zbc_ata_get_qword(&buf[32]));
		domains[i].zbm_flags = zbc_ata_get_dword(&buf[42]);
		domains[i].zbm_type = buf[40];
		if (!(domains[i].zbm_flags & ZBC_ZDF_VALID_ZONE_TYPE)) {
#ifndef REJECT_OLD_DOMAIN_FORMAT
			zbc_warning("%s: Zone type 0x%x not valid for domain %u, flags=0x%x\n",
				    dev->zbd_filename, domains[i].zbm_type,
				    domains[i].zbm_id, domains[i].zbm_flags);
#else
			zbc_error("%s: Zone type 0x%x not valid for domain %u, flags=0x%x\n",
				  dev->zbd_filename, domains[i].zbm_type,
				  domains[i].zbm_id, domains[i].zbm_flags);
			/*
			 * We could have done REPORT ZONES on domain start LBA
			 * to get the type, but if the bit is not set, then the
			 * type is likely to be variable across the domain and
			 * we don't support this functionality.
			 */
			ret = -EIO;
			goto out;
#endif
		}

		if (domains[i].zbm_type != ZBC_ZT_CONVENTIONAL &&
		    domains[i].zbm_type != ZBC_ZT_SEQUENTIAL_REQ &&
		    domains[i].zbm_type != ZBC_ZT_SEQUENTIAL_PREF &&
		    domains[i].zbm_type != ZBC_ZT_SEQ_OR_BEF_REQ) {
			zbc_error("%s: Unknown zone type 0x%x of domain %u, flags=0x%x\n",
				  dev->zbd_filename, domains[i].zbm_type,
				  domains[i].zbm_id, domains[i].zbm_flags);
			ret = -EINVAL;
			goto out;
		}

		buf += ZBC_RPT_DOMAINS_RECORD_SIZE;
	}

	/* Return the number of domain descriptors */
	ret = nd;

out:
	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Report device zone realm configuration.
 */
static int zbc_ata_report_realms(struct zbc_device *dev, uint64_t sector,
				 enum zbc_realm_report_options ro,
				 struct zbc_zone_realm *realms,
				 unsigned int *nr_realms)
{
	struct zbc_zone_domain *domains = NULL, *d;
	struct zbc_realm_item *ri;
	uint8_t const *buf, *ptr;
	uint64_t zone_size, lba = zbc_dev_sect2lba(dev, sector);
#ifdef ZBC_STANDARD_RPT_REALMS
	uint64_t next;
#endif
	size_t bufsz = ZBC_RPT_REALMS_HEADER_SIZE;
	unsigned int i, nr = 0, desc_len;
	struct zbc_sg_cmd cmd;
	int ret, j, nr_domains;

	/*
	 * Always get zone domains first. Allocate the buffer for
	 * ZBC_NR_ZONE_TYPES domains since we will be only able to
	 * process this many.
	 */
	nr_domains = ZBC_NR_ZONE_TYPES;
	domains = calloc(nr_domains, sizeof(struct zbc_zone_domain));
	if (!domains)
		return -ENOMEM;

	nr_domains = zbc_ata_report_domains(dev, 0LL, ZBC_RZD_RO_ALL, domains,
					    nr_domains);
	if (nr_domains < 0) {
		free(domains);
		return nr_domains;
	}
	if (nr_domains > ZBC_NR_ZONE_TYPES) {
		zbc_warning("%s: Device has %i domains, only %u are supported\n",
			    dev->zbd_filename, nr_domains, ZBC_NR_ZONE_TYPES);

		nr_domains = ZBC_NR_ZONE_TYPES;
	}
	if (*nr_realms)
		bufsz += (size_t)*nr_realms * ZBC_RPT_REALMS_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and initialize REPORT REALMS command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, bufsz);
	if (ret != 0) {
		free(domains);
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
	 * | 3   |                  features (15:8) Reporting options                    |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |                   features (7:0), action (06h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5-6 |                                count                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7-12|                             LBA (ZONE ID)                             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                   Device, bit 6 shall be set to 1                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command (4Ah)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                             Control                                   |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x06 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	/* Fill RO, ZONE ID, AF, Count and Device */
	cmd.cdb[3] = ro & 0x3f;
	cmd.cdb[4] = ZBC_ATA_REPORT_REALMS_AF;
	cmd.cdb[5] = ((bufsz / 512) >> 8) & 0xff;
	cmd.cdb[6] = (bufsz / 512) & 0xff;
	zbc_ata_put_lba(cmd.cdb, lba);
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_IN;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret) {
		zbc_ata_get_sense_data(dev, &cmd, ret);
		goto out;
	}

	if (cmd.bufsz < ZBC_RPT_REALMS_HEADER_SIZE) {
		zbc_error("%s: Not enough REPORT REALMS data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_RPT_REALMS_HEADER_SIZE,
			  cmd.bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get the number of realm descriptors from the header */
	buf = cmd.buf;
	nr = zbc_ata_get_dword(&buf[0]);

	if (!realms || !nr)
		goto out;

	/* Find the number of zone realm descriptors to fill */
	if (nr > *nr_realms)
		nr = *nr_realms;

#ifdef ZBC_STANDARD_RPT_REALMS
	desc_len = zbc_ata_get_dword(&buf[4]);
	if (!desc_len)
		goto oldrealms; /* The field is reserved pre ZDr4, so it has to be 0 */
	next = zbc_ata_get_qword(&buf[8]);
	if (next) {
		zbc_error("%s: NEXT REALM LOCATOR is not yet supported",
			  dev->zbd_filename);
		ret = -ENXIO; /* FIXME handle */
		goto out;
	}

	bufsz = (cmd.bufsz - ZBC_RPT_REALMS_HEADER_SIZE) / desc_len;
	if (nr > bufsz)
		nr = bufsz;

	/* Get zone realm descriptors */
	buf += ZBC_RPT_REALMS_HEADER_SIZE;
	for (i = 0; i < nr; i++, realms++) {
		realms->zbr_number = zbc_ata_get_dword(buf);
		realms->zbr_restr = zbc_ata_get_word(&buf[4]);
		realms->zbr_dom_id = buf[7];
		if (realms->zbr_dom_id < ZBC_NR_ZONE_TYPES)
			realms->zbr_type = domains[realms->zbr_dom_id].zbm_type;
		realms->zbr_nr_domains = nr_domains;
		ptr = buf + ZBC_RPT_REALMS_DESC_OFFSET;
		/* FIXME don't use nr_domains, use desc_len to limit iteration */
		for (j = 0; j < nr_domains; j++) {
			ri = &realms->zbr_ri[j];
			ri->zbi_end_sector =
					zbc_dev_lba2sect(dev, zbc_ata_get_qword(ptr + 8));
			if (ri->zbi_end_sector) {
				realms->zbr_actv_flags |= (1 << j);
				d = &domains[j];
				ri->zbi_dom_id = j;
				ri->zbi_type = d->zbm_type;
				ri->zbi_start_sector =
					zbc_dev_lba2sect(dev, zbc_ata_get_qword(ptr));
				if (d->zbm_nr_zones)
					zone_size = zbc_zone_domain_zone_size(d);
				else
					zone_size = 0;
				if (zone_size) {
					ri->zbi_length = (ri->zbi_end_sector + 1 - ri->zbi_start_sector) /
							 zone_size;
				}
			}
			ptr += ZBC_RPT_REALMS_SE_DESC_SIZE;
		}

		buf += desc_len;
	}
	goto out;

oldrealms:
	desc_len = ZBC_RPT_REALMS_RECORD_SIZE;
#else
	desc_len = ZBC_RPT_REALMS_RECORD_SIZE;
	bufsz = (cmd.bufsz - ZBC_RPT_REALMS_HEADER_SIZE) / desc_len;
	if (nr > bufsz)
		nr = bufsz;
#endif

	/* Get zone realm descriptors */
	buf += ZBC_RPT_REALMS_HEADER_SIZE;
	for (i = 0; i < nr; i++, realms++) {
		realms->zbr_dom_id = buf[0];
		if (realms->zbr_dom_id < ZBC_NR_ZONE_TYPES)
			realms->zbr_type = domains[realms->zbr_dom_id].zbm_type;
		realms->zbr_actv_flags = buf[1];
		realms->zbr_number = zbc_ata_get_word(&buf[2]);
		realms->zbr_nr_domains = nr_domains;
		ptr = buf + ZBC_RPT_REALMS_DESC_OFFSET;
		for (j = 0; j < ZBC_NR_ZONE_TYPES; j++) {
			if (realms->zbr_actv_flags & (1 << j)) {
				d = &domains[j];
				ri = &realms->zbr_ri[j];
				ri->zbi_dom_id = j;
				ri->zbi_type = d->zbm_type;
				ri->zbi_start_sector =
					zbc_dev_lba2sect(dev, zbc_ata_get_qword(ptr));
				ri->zbi_end_sector =
					zbc_dev_lba2sect(dev, zbc_ata_get_qword(ptr + 8));
				if (d->zbm_nr_zones)
					zone_size = zbc_zone_domain_zone_size(d);
				else
					zone_size = 0;
				if (zone_size) {
					ri->zbi_length = (ri->zbi_end_sector + 1 - ri->zbi_start_sector) /
							 zone_size;
				}
			}
			ptr += ZBC_RPT_REALMS_SE_DESC_SIZE;
		}

		buf += ZBC_RPT_REALMS_RECORD_SIZE;
	}

out:
	if (domains)
		free(domains);

	/* Return the number of descriptors */
	*nr_realms = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Read or set Zone Domains configuration parameters.
 */
static int zbc_ata_dev_control(struct zbc_device *dev,
			       struct zbc_zd_dev_control *ctl, bool set)
{
	struct zbc_device_info *di = &dev->zbd_info;
	uint8_t buf[512];
	uint64_t qwd;
	int ret;

	if (!set) {
		memset(ctl, 0, sizeof(*ctl));

		/* Get zoned block device information */
		ret = zbc_ata_read_log(dev,
				       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
				       ZBC_ATA_ZONED_DEVICE_INFORMATION_PAGE,
				       buf,
				       sizeof(buf));
		if (ret < 0)
			return ret;

		qwd = zbc_ata_get_qword(&buf[64]);
		ctl->zbt_nr_zones = qwd & 0xffffff;

		qwd = zbc_ata_get_qword(&buf[8]);
		ctl->zbt_urswrz = qwd & 0x01;

		qwd = zbc_ata_get_qword(&buf[56]);
		ctl->zbt_max_activate = (qwd >> 32) & 0xffff;

		return ret;
	}

	if (ctl->zbt_nr_zones != 0xffffffff) {
		if (!(di->zbd_flags & ZBC_ZA_CONTROL_SUPPORT)) {
			zbc_error("%s: ZA control not supported\n",
				  dev->zbd_filename);
			return -ENOTSUP;
		}
		ret = zbc_ata_set_features(dev,
					   ZBC_ATA_ZONE_ACTIVATION_CONTROL,
					   ctl->zbt_nr_zones, 0);
		if (ret != 0) {
			zbc_error("%s: Failed to set FSNOZ %u\n",
				  dev->zbd_filename, ctl->zbt_nr_zones);
			return ret;
		}
	}

	if (ctl->zbt_urswrz != 0xff) {
		if (!(di->zbd_flags & ZBC_URSWRZ_SET_SUPPORT)) {
			zbc_error("%s: URSWRZ update not supported\n",
				  dev->zbd_filename);
			return -ENOTSUP;
		}
		ret = zbc_ata_set_features(dev,
					   ZBC_ATA_UPDATE_URSWRZ,
					   0, ctl->zbt_urswrz);
		if (ret != 0) {
			zbc_error("%s: Failed to set USRWRZ %u\n",
				  dev->zbd_filename, ctl->zbt_urswrz);
			return ret;
		}
	}

	if (ctl->zbt_max_activate != 0xffff) { /* FIXME 32 bit? */
		if (!(di->zbd_flags & ZBC_MAXACT_SET_SUPPORT)) {
			zbc_error("%s: MAX ACTIVATION set not supported\n",
				  dev->zbd_filename);
			return -ENOTSUP;
		}
		ret = zbc_ata_set_features(dev,
					   ZBC_ATA_UPDATE_MAX_ACTIVATION,
					   ctl->zbt_max_activate, 0);
		if (ret != 0) {
			zbc_error("%s: Failed to set MAX ACTIVATION %u\n",
				  dev->zbd_filename, ctl->zbt_max_activate);
			return ret;
		}
	}

	return 0;
}

static int zbc_ata_zone_activate_aux(struct zbc_device *dev, bool all,
				     bool query, uint64_t sector,
				     uint32_t nr_zones, unsigned int domain_id,
				     struct zbc_actv_res *actv_recs,
				     unsigned int *nr_actv_recs)
{
	/* FIXME support */
	zbc_warning("%s: Setting NOZSRC is not supported for ATA\n",
		    dev->zbd_filename);
	return -ENOTSUP;
}

static int zbc_ata_zone_activate_noaux(struct zbc_device *dev, bool all,
				       bool query, uint64_t sector,
				       unsigned int domain_id,
				       struct zbc_actv_res *actv_recs,
				       unsigned int *nr_actv_recs)
{
	size_t bufsz = ZBC_ACTV_RES_HEADER_SIZE;
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int i, nr = 0;
	struct zbc_sg_cmd cmd;
	uint16_t stat;
	uint8_t const *buf;
	int ret;

	if (*nr_actv_recs)
		bufsz += (size_t)*nr_actv_recs * ZBC_ACTV_RES_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and initialize ZONE ACTIVATE/QUERY command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_ATA16, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* This operation can be quite lengthy... */
	cmd.io_hdr.timeout = 120000;

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
	 * | 3   |                  features (15:8) OTHER DOMAIN ID                      |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   |            4:0 ZM ACTION (08h/09h), 5 - NOZSRC=0, 7 - ALL             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5-6 |                         RETURN PAGE COUNT                             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7-12|                           LBA (ZONE ID)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  |                   Device, bit 6 shall be set to 1                     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Command (4Ah)                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                             Control                                   |
	 * +=============================================================================+
	 */
	cmd.io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	cmd.cdb[0] = ZBC_SG_ATA16_CDB_OPCODE;
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x06 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	/* Fill AF, Features, Count and Device */
	cmd.cdb[3] = domain_id; /* Domain ID to activate */
	cmd.cdb[4] = query ? ZBC_ATA_ZONE_QUERY_AF : ZBC_ATA_ZONE_ACTIVATE_AF;
	if (all)
		cmd.cdb[4] |= 0x80; /* All */
	cmd.cdb[5] = ((bufsz / 512) >> 8) & 0xff;
	cmd.cdb[6] = (bufsz / 512) & 0xff;
	zbc_ata_put_lba(cmd.cdb, lba);
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_IN;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret) {
		/* Get sense data if enabled */
		zbc_ata_get_sense_data(dev, &cmd, ret);
		goto out;
	}

	buf = cmd.buf;

	if (zbc_log_level >= ZBC_LOG_DEBUG) {
		size_t sz = ZBC_ACTV_RES_HEADER_SIZE + zbc_ata_get_dword(buf);
		if (sz > cmd.bufsz)
			sz = cmd.bufsz;
		zbc_debug("%s: %s REPLY (%zd/%zd B):\n",
				(query ? "QUERY" : "ACTIVATE"),
				dev->zbd_filename,
				sz, cmd.bufsz);
		if (sz > 64)
			sz = 64;	/* limit the amount printed */
		zbc_sg_print_bytes(dev, buf, sz);
	}

	/*
	 * Collect the status bits and the Zone ID With Unmet
	 * Prerequisites if ACTIVATED bit is not set.
	 */
	stat = buf[9];
	if (buf[8] & 0x01) /* ACTIVATED */
		stat |= 0x8000;
	if (buf[8] & 0x40) /* ZIWUP valid */
		stat |= 0x4000;

	if (((stat & 0x8000) == 0 && !query) ||
	    (stat & 0x4000) != 0) {
		zerrno.err_za = stat;
		if (stat & 0x4000) /* ZIWUP field valid */
			zerrno.err_cbf =
				 zbc_dev_lba2sect(dev,
						  zbc_ata_get_lba(&buf[24]));
		zbc_warning("%s: Zones %s activated {ERR=0x%04x CBF=%"PRIu64" (%svalid)}\n",
			    dev->zbd_filename, query ? "will not be" : "not",
			    zerrno.err_za, zerrno.err_cbf,
			    ((zerrno.err_za & 0x4000) ? "" : "in"));
		ret = -EIO;

		/* There still might be descriptors returned, try to read them */
	}

	/* Get the number of records in activation results */
	if (!actv_recs) {
		nr = zbc_ata_get_dword(buf) / ZBC_ACTV_RES_RECORD_SIZE;
		goto out;
	}
	nr = zbc_ata_get_dword(&buf[4]) / ZBC_ACTV_RES_RECORD_SIZE;
	if (!nr)
		goto out;
	/*
	 * Only get as many activation results records
	 * as the allocated buffer allows.
	 */
	if (nr > *nr_actv_recs)
		nr = *nr_actv_recs;

	/* Get the activation results records */
	buf += ZBC_ACTV_RES_HEADER_SIZE;
	for (i = 0; i < nr; i++) {
		actv_recs[i].zbe_type = buf[0] & 0x0f;
		actv_recs[i].zbe_condition = (buf[1] >> 4) & 0x0f;
		actv_recs[i].zbe_domain = buf[2];
		actv_recs[i].zbe_nr_zones = zbc_ata_get_qword(&buf[8]);
		actv_recs[i].zbe_start_zone =
			zbc_dev_lba2sect(dev, zbc_ata_get_lba(&buf[16]));

		buf += ZBC_ACTV_RES_RECORD_SIZE;
	}

out:
	/* Return the number of descriptors */
	*nr_actv_recs = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

static int zbc_ata_zone_query_activate(struct zbc_device *dev, bool zsrc,
				       bool all, bool use_32_byte_cdb,
				       bool query, uint64_t sector,
				       uint32_t nr_zones,
				       unsigned int domain_id,
				       struct zbc_actv_res *actv_recs,
				       unsigned int *nr_actv_recs)
{
	if (all)
		zsrc = false;

	return zsrc ?
	       zbc_ata_zone_activate_aux(dev, all, query, sector, nr_zones,
					 domain_id, actv_recs, nr_actv_recs) :
	       zbc_ata_zone_activate_noaux(dev, all, query, sector, domain_id,
					   actv_recs, nr_actv_recs);
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
 * Get zoned device information (the maximum and optimal number
 * of open zones, read restriction, etc). Data log 30h, page 09h.
 */
static int zbc_ata_get_zoned_device_info(struct zbc_device *dev)
{
	struct zbc_device_info *di = &dev->zbd_info;
	uint64_t qwd;
	uint8_t buf[512];
	uint32_t val;
	int ret;

	/* Get zoned block device information */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_ZONED_DEVICE_INFORMATION_PAGE,
			       buf,
			       sizeof(buf));
	if (ret < 0)
		return ret;

	/*
	 * Check if Zone Domains/Realms command set is supported.
	 * If this is the case, pick up all the related values.
	 */
	qwd = zbc_ata_get_qword(&buf[56]);
	di->zbd_flags |= (qwd & 0x01ULL) ? ZBC_ZONE_DOMAINS_SUPPORT : 0;
	di->zbd_flags |= (qwd & 0x02ULL) ? ZBC_ZONE_REALMS_SUPPORT : 0;
	if ((di->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) ||
	    (di->zbd_flags & ZBC_ZONE_REALMS_SUPPORT)) {

		if (di->zbd_model != ZBC_DM_STANDARD) {
			zbc_error("%s: Invalid model %u if ATA ZD bit is set\n",
				  dev->zbd_filename, dev->zbd_info.zbd_model);
			return -EINVAL;
		}
		zbc_debug("%s: Zone Domains ATA device detected\n",
			  dev->zbd_filename);

		di->zbd_model = ZBC_DM_HOST_MANAGED;
		di->zbd_flags |= (qwd & 0x4ULL) ? ZBC_URSWRZ_SET_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x8ULL) ? ZBC_ZA_CONTROL_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x10ULL) ? ZBC_NOZSRC_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x20ULL) ? ZBC_REPORT_REALMS_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x40ULL) ? ZBC_MAXACT_SET_SUPPORT : 0;
		di->zbd_max_activation = (qwd >> 32) & 0xffff;

		if (di->zbd_flags & ZBC_ZONE_REALMS_SUPPORT &&
		    !(di->zbd_flags & ZBC_REPORT_REALMS_SUPPORT)) {
			zbc_info("%s: Realm device doesn't support REPORT REALMS\n",
				 dev->zbd_filename);
		}

		/* Get Subsequent Number of Zones */
		qwd = zbc_ata_get_qword(&buf[64]);
		di->zbd_snoz = qwd & 0xfffffffULL;

		/* Check what zone types are supported */
		qwd = zbc_ata_get_qword(&buf[72]);
		di->zbd_flags |= (qwd & 0x01ULL) ? ZBC_CONV_ZONE_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x02ULL) ? ZBC_SEQ_PREF_ZONE_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x04ULL) ? ZBC_SEQ_REQ_ZONE_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x08ULL) ? ZBC_SOBR_ZONE_SUPPORT : 0;
		di->zbd_flags |= (qwd & 0x10ULL) ? ZBC_GAP_ZONE_SUPPORT : 0;

	} else if (di->zbd_model == ZBC_DM_STANDARD) {
		zbc_debug("%s: Standard ATA device detected\n",
			  dev->zbd_filename);
	}

	if (!zbc_dev_is_zoned(dev))
		/* A standard ATA device without ZD/ZR support, bail */
		return -ENXIO;

	/* URSWRZ (unrestricted read write sequential required zone) flag */
	di->zbd_flags |= (zbc_ata_get_qword(&buf[8]) & 0x01) ?
		ZBC_UNRESTRICTED_READ : 0;

	/* Maximum number of zones for resource management */
	if (di->zbd_model == ZBC_DM_HOST_AWARE) {

		val = zbc_ata_get_qword(&buf[24]) & 0xffffffff;
		if (!val) {
			/* Handle this case as "not reported" */
			zbc_warning("%s: invalid optimal number of open "
				    "sequential write preferred zones\n",
				    dev->zbd_filename);
			val = ZBC_NOT_REPORTED;
		}
		di->zbd_opt_nr_open_seq_pref = val;

		val = zbc_ata_get_qword(&buf[32]) & 0xffffffff;
		if (!val) {
			/* Handle this case as "not reported" */
			zbc_warning("%s: invalid optimal number of randomly "
				    "written sequential write preferred zones\n",
				    dev->zbd_filename);
			val = ZBC_NOT_REPORTED;
		}
		di->zbd_opt_nr_non_seq_write_seq_pref = val;

		di->zbd_max_nr_open_seq_req = 0;

	} else {

		di->zbd_opt_nr_open_seq_pref = 0;
		di->zbd_opt_nr_non_seq_write_seq_pref = 0;

		val = zbc_ata_get_qword(&buf[40]) & 0xffffffff;
		if (!val) {
			/* Handle this case as "no limit" */
			zbc_warning("%s: invalid maximum number of open "
				    "sequential write required zones\n",
				    dev->zbd_filename);
			val = ZBC_NO_LIMIT;
		}
		di->zbd_max_nr_open_seq_req = val;

	}

	return 0;
}

/**
 * Read from a ZAC device using READ DMA EXT packed
 * in an ATA PASSTHROUGH command.
 */
static ssize_t zbc_ata_native_preadv(struct zbc_device *dev,
				     const struct iovec *iov, int iovcnt,
				     uint64_t offset)
{
	size_t sz = zbc_iov_count(iov, iovcnt);
	size_t count = sz >> 9;
	uint32_t lba_count = zbc_dev_sect2lba(dev, count);
	uint64_t lba = zbc_dev_sect2lba(dev, offset);
	struct zbc_sg_cmd cmd;
	ssize_t ret;

	/* Check */
	if (count > 65536) {
		zbc_error("%s: Read operation too large "
			  "(limited to 65536 x 512 B sectors)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Initialize the command */
	ret = zbc_sg_vcmd_init(dev, &cmd, ZBC_SG_ATA16, iov, iovcnt);
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
	zbc_ata_put_lba(cmd.cdb, lba);
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_READ_DMA_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret)
		zbc_ata_get_sense_data(dev, &cmd, ret);
	else
		ret = (sz - cmd.io_hdr.resid) >> 9;

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Vector read from a ZAC device.
 */
static ssize_t zbc_ata_preadv(struct zbc_device *dev,
			      const struct iovec *iov, int iovcnt,
			      uint64_t offset)
{
	if (dev->zbd_drv_flags & ZBC_ATA_USE_SBC)
		return zbc_scsi_preadv(dev, iov, iovcnt, offset);

	return zbc_ata_native_preadv(dev, iov, iovcnt, offset);
}

/**
 * Write to a ZAC device using WRITE DMA EXT packed
 * in an ATA PASSTHROUGH command.
 */
static ssize_t zbc_ata_native_pwritev(struct zbc_device *dev,
				      const struct iovec *iov, int iovcnt,
				      uint64_t offset)
{
	size_t sz = zbc_iov_count(iov, iovcnt);
	size_t count = sz >> 9;
	uint32_t lba_count = zbc_dev_sect2lba(dev, count);
	uint64_t lba = zbc_dev_sect2lba(dev, offset);
	struct zbc_sg_cmd cmd;
	int ret;

	/* Check */
	if (count > 65536) {
		zbc_error("%s: Write operation too large (limited to 65536 x 512 B sectors)\n",
			  dev->zbd_filename);
		return -EINVAL;
	}

	/* Initialize the command */
	ret = zbc_sg_vcmd_init(dev, &cmd, ZBC_SG_ATA16, iov, iovcnt);
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
	zbc_ata_put_lba(cmd.cdb, lba);
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_WRITE_DMA_EXT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret)
		zbc_ata_get_sense_data(dev, &cmd, ret);
	else
		ret = (sz - cmd.io_hdr.resid) >> 9;

	/* Done */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Vector write to a ZAC device.
 */
static ssize_t zbc_ata_pwritev(struct zbc_device *dev,
			       const struct iovec *iov, int iovcnt,
			       uint64_t offset)
{
	if (dev->zbd_drv_flags & ZBC_ATA_USE_SBC)
		return zbc_scsi_pwritev(dev, iov, iovcnt, offset);

	return zbc_ata_native_pwritev(dev, iov, iovcnt, offset);
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
	cmd.io_hdr.timeout *= 2;
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
static int zbc_ata_do_rpt_zones(struct zbc_device *dev, uint64_t sector,
				enum zbc_zone_reporting_options ro, uint64_t *max_lba,
				struct zbc_zone *zones, unsigned int *nr_zones,
				size_t bufsz)
{
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int i, nz = 0, buf_nz;
	struct zbc_sg_cmd cmd;
	uint8_t *buf;
	int ret;

	/* Initialize the command */
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
	zbc_ata_put_lba(cmd.cdb, lba);
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_IN;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret) {
		zbc_ata_get_sense_data(dev, &cmd, ret);
		goto out;
	}

	if (cmd.bufsz < ZBC_ZONE_DESCRIPTOR_OFFSET) {
		zbc_error("%s: Not enough REPORT ZONES data received "
			  "(need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_ZONE_DESCRIPTOR_OFFSET,
			  cmd.bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get number of zones in result */
	buf = (uint8_t *) cmd.buf;
	nz = zbc_ata_get_dword(buf) / ZBC_ZONE_DESCRIPTOR_LENGTH;
	if (max_lba)
		*max_lba = zbc_ata_get_qword(&buf[8]);

	if (!zones || !nz)
		goto out;

	/* Get zone info */
	if (nz > *nr_zones)
		nz = *nr_zones;

	buf_nz = (cmd.bufsz - ZBC_ZONE_DESCRIPTOR_OFFSET)
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
		if (zbc_zone_sequential(&zones[i]) || zbc_zone_sobr(&zones[i]))
			zones[i].zbz_write_pointer =
				zbc_dev_lba2sect(dev,
						 zbc_ata_get_qword(&buf[24]));
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
 * Get device zone information.
 */
static int zbc_ata_report_zones(struct zbc_device *dev, uint64_t sector,
				enum zbc_zone_reporting_options ro,
				struct zbc_zone *zones, unsigned int *nr_zones)
{
	size_t bufsz = 0;

	if (zones) {
		bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
		if (bufsz > (*nr_zones + 1) * 64)
			bufsz = (*nr_zones + 1) * 64;
	}
	if (bufsz < dev->zbd_report_bufsz_min)
		bufsz = dev->zbd_report_bufsz_min;
	else if (bufsz & dev->zbd_report_bufsz_mask)
		bufsz = (bufsz + dev->zbd_report_bufsz_mask) &
			~dev->zbd_report_bufsz_mask;

	return zbc_ata_do_rpt_zones(dev, sector, ro, NULL, zones, nr_zones,
				    bufsz);
}

/**
 * Zone(s) operation.
 */
static int zbc_ata_zone_op(struct zbc_device *dev, uint64_t sector,
			   unsigned int count, enum zbc_zone_op op,
			   unsigned int flags)
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

	/* Initialize command */
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
		zbc_ata_put_lba(cmd.cdb, lba);
	}
	cmd.cdb[5] = (count >> 8) & 0xff;
	cmd.cdb[6] = count & 0xff;
	cmd.cdb[13] = 1 << 6;
	cmd.cdb[14] = ZBC_ATA_ZAC_MANAGEMENT_OUT;

	/* Execute the command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret)
		zbc_ata_get_sense_data(dev, &cmd, ret);

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

	/* Initialize command */
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
		zbc_debug("%s: Standard or Zone Domains device\n",
			  dev->zbd_filename);
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
	if (!(zoned & (1ULL << 63)))
		zoned = 0;

	zoned = zoned & 0x03;
	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED) {
		if (zbc_test_mode(dev) && zoned != 0) {
			zbc_error("%s: Invalid host-managed device ZONED field 0x%02x\n",
				  dev->zbd_filename, (unsigned int)zoned);
			ret = -EIO;
			goto out;
		} else if (zoned != 0) {
			zbc_warning("%s: Invalid host-managed device ZONED field 0x%02x\n",
				    dev->zbd_filename, (unsigned int)zoned);
		}
		goto out;
	}

	switch (zoned) {

	case 0x00:
		dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
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
int zbc_ata_get_capacity(struct zbc_device *dev)
{
	uint8_t buf[512];
	uint64_t qword;
	int logical_per_physical;
	uint64_t max_lba;
	unsigned int nr_zones = 0;
	int ret;

	/* Get capacity log page */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_CAPACITY_PAGE,
			       buf,
			       sizeof(buf));
	if (ret != 0) {
		zbc_error("%s: Get capacity page failed\n",
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

	if (zbc_dev_is_zdr(dev) != 0) {
		/*
		 * The capacity represents only the space used by
		 * conventional zones at the beginning of the device.
		 * To get the entire device capacity, we need to get
		 * the last LBA of the last zone of the device.
		 */
		ret = zbc_ata_do_rpt_zones(dev, 0,
					   ZBC_RZ_RO_ALL | ZBC_RZ_RO_PARTIAL,
					   &max_lba,  NULL, &nr_zones,
					   dev->zbd_report_bufsz_min);
		if (ret != 0)
			return ret;

		/* Set the drive capacity using the reported max LBA */
		dev->zbd_info.zbd_lblocks = max_lba + 1;
	}

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
	struct iovec iov = { buf, sizeof(buf) >> 9 };
	struct zbc_device_info *di = &dev->zbd_info;
	int ret;

	if ((di->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) ||
	    (di->zbd_flags & ZBC_ZONE_REALMS_SUPPORT)) {
		/*
		 * If this is a DHSMR device, the kernel is likely
		 * to support SAT, no additional check is needed.
		 */
		dev->zbd_drv_flags |= ZBC_ATA_USE_SBC;
		zbc_debug("%s: ZD/ZR device, using SBC SAT for read/write/flush\n",
			  dev->zbd_filename);
	} else if (di->zbd_flags & ZBC_UNRESTRICTED_READ) {
		ret = zbc_scsi_preadv(dev, &iov, 1, 0);
		if (ret == 8) {
			dev->zbd_drv_flags |= ZBC_ATA_USE_SBC;
			zbc_debug("%s: Using SCSI commands for read/write/flush\n",
				  dev->zbd_filename);
		} else {
			zbc_debug("%s: read error %d, not using SBC SAT\n",
				  dev->zbd_filename, ret);
		}
	} else {
		zbc_debug("%s: not using SBC SAT\n", dev->zbd_filename);
	}
}

/**
 * Receive ZBD statistic counters from the device.
 */
static int zbc_ata_get_stats(struct zbc_device * dev,
			     struct zbc_zoned_blk_dev_stats *stats)
{
	/* FIXME implement */
	return -ENXIO;
}

/**
 * Get a device information (capacity & sector sizes).
 */
static int zbc_ata_get_dev_info(struct zbc_device *dev)
{
	int ret;

	/* Always use 512B aligned zone report buffers */
	dev->zbd_report_bufsz_min = 512;
	dev->zbd_report_bufsz_mask = dev->zbd_report_bufsz_min - 1;

	/* Make sure the device is ready */
	ret = zbc_sg_test_unit_ready(dev);
	if (ret != 0)
		return ret;

	/* Get device model */
	ret = zbc_ata_classify(dev);
	if (ret != 0)
		return ret;

	/* Get zoned device information */
	ret = zbc_ata_get_zoned_device_info(dev);
	if (ret != 0)
		return ret;

	/* Get capacity information */
	ret = zbc_ata_get_capacity(dev);
	if (ret != 0)
		return ret;

	/* Get vendor information */
	zbc_ata_vendor_id(dev);

	/* Get maximum command size */
	zbc_sg_get_max_cmd_blocks(dev);

	/* Check if we have a functional SAT for read/write */
	if (!zbc_test_mode(dev))
		zbc_ata_test_sbc_sat(dev);

	return 0;
}

/**
 * Check sense data reporting is enabled. ZAC mandates it then it is expected
 * already enabled. In case it is disabled, call set feature command to enable.
 */
static void zbc_ata_enable_sense_data_reporting(struct zbc_device *dev)
{
	uint8_t buf[512];
	int ret;

	/* Get current settings page */
	ret = zbc_ata_read_log(dev,
			       ZBC_ATA_IDENTIFY_DEVICE_DATA_LOG_ADDR,
			       ZBC_ATA_CURRENT_SETTINGS_PAGE,
			       buf,
			       sizeof(buf));
	if (ret != 0) {
		zbc_debug("%s: Get current settings log page failed %d\n",
			  dev->zbd_filename, ret);
		return;
	}

	/*
	 * Sense data reporting should be enabled as mandated by ACS. If it is,
	 * nothing needs to be done. Otherwise, warn about it and try to enable
	 * it.
	 */
	if (zbc_ata_get_qword(&buf[8]) & (1ULL << 10))
		return;

	zbc_warning("%s: Sense data reporting is disabled\n",
		    dev->zbd_filename);
	zbc_warning("%s: ACS mandates sense data reporting being enabled\n",
		    dev->zbd_filename);
	zbc_warning("%s: Trying to enable sense data reporting\n",
		    dev->zbd_filename);
	ret = zbc_ata_set_features(dev,
			ZBC_ATA_ENABLE_SENSE_DATA_REPORTING, 0, 0x01);
	if (ret != 0) {
		zbc_warning("%s: Enable sense data reporting failed %d\n",
			    dev->zbd_filename, ret);
		zbc_warning("%s: Detailed error reporting may not work\n",
			    dev->zbd_filename);
	}

	return;
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
	if (fd < 0) {
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

	/* Set device descriptor */
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
	if (ret != 0) {
		zbc_error("%s: Getting device info failed, err %i\n",
			  filename, ret);
		ret = -ENXIO;
		goto out_free_filename;
	}

	zbc_ata_enable_sense_data_reporting(dev);

	*pdev = dev;

	zbc_debug("%s: ########## ATA driver succeeded ##########\n\n",
		  filename);

	return 0;

out_free_filename:
	free(dev->zbd_filename);

out_free_dev:
	free(dev);

out:
	if (fd >= 0)
		close(fd);

	zbc_debug("%s: ########## ATA driver failed %d ##########\n\n",
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
struct zbc_drv zbc_ata_drv = {
	.flag			= ZBC_O_DRV_ATA,
	.zbd_open		= zbc_ata_open,
	.zbd_close		= zbc_ata_close,
	.zbd_dev_control	= zbc_ata_dev_control,
	.zbd_preadv		= zbc_ata_preadv,
	.zbd_pwritev		= zbc_ata_pwritev,
	.zbd_flush		= zbc_ata_flush,
	.zbd_report_zones	= zbc_ata_report_zones,
	.zbd_zone_op		= zbc_ata_zone_op,
	.zbd_report_domains	= zbc_ata_report_domains,
	.zbd_report_realms	= zbc_ata_report_realms,
	.zbd_zone_query_actv	= zbc_ata_zone_query_activate,
	.zbd_get_stats		= zbc_ata_get_stats,
};

