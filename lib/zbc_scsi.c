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
 *          Christophe Louargant (christophe.louargant@wdc.com)
 */

#include "zbc.h"
#include "zbc_sg.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

/**
 * Number of bytes in a Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_LENGTH	64

/**
 * Number of bytes in the buffer before the first Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_OFFSET	64

/**
 * REPORT ZONE DOMAINS ouput header size.
 */
#define ZBC_RPT_DOMAINS_HEADER_SIZE	64

/**
 * REPORT ZONE DOMAINS output descriptor size.
 */
#define ZBC_RPT_DOMAINS_RECORD_SIZE	96

/**
 * REPORT REALMS ouput header size.
 */
#define ZBC_RPT_REALMS_HEADER_SIZE	64

/**
 * REPORT REALMS output zone realm descriptor definitions.
 */
#define ZBC_RPT_REALMS_RECORD_SIZE	128
#define ZBC_RPT_REALMS_DESC_OFFSET	16
#ifdef ZBC_NEW_RPT_REALMS
 #define ZBC_RPT_REALMS_SE_DESC_SIZE	32
#endif
#define ZBC_RPT_REALMS_ITEM_SIZE	20

/**
 * SCSI commands reply length.
 */
#define ZBC_SCSI_INQUIRY_BUF_LEN	96
#define ZBC_SCSI_VPD_PAGE_B1_LEN	64
#define ZBC_SCSI_VPD_PAGE_B6_LEN	64
#define ZBC_SCSI_READ_CAPACITY_BUF_LEN	32

/**
 * Zone activation results header size.
 */
#define ZBC_ACTV_RES_HEADER_SIZE	64

/**
 * Zone activation results record size.
 */
#define ZBC_ACTV_RES_RECORD_SIZE	24

/**
 * ZONE DOMAINS mode page size
 */
#define ZBC_SCSI_MODE_PG_SIZE		256

/**
 * ZONE DOMAINS mode page minimum size
 */
#define ZBC_SCSI_MIN_MODE_PG_SIZE	8

/**
 * Data offset from the beginning of MODE SENSE/SELSECT data buffer
 * to the first byte of the actual page.
 */
#define ZBC_MODE_PAGE_OFFSET		12

/**
 * ZONE DOMAINS mode page and subpage numbers.
 * FIXME The values below are in vendor-specific range and will change
 */
#define ZBC_ZONE_DOM_MODE_PG		0x3d
#define ZBC_ZONE_DOM_MODE_SUBPG		0x08

/**
 * REPORT MUTATIONS output header size
 */
#define ZBC_MUTATE_RPT_HEADER_SIZE	32

/**
 * REPORT MUTATIONS output record size
 */
#define ZBC_MUTATE_RPT_RECORD_SIZE	8

/**
 * Fill the buffer with the result of INQUIRY command.
 * @buf must be at least ZBC_SG_INQUIRY_REPLY_LEN bytes long.
 */
static int zbc_scsi_inquiry(struct zbc_device *dev,
			    uint8_t page,
			    void *buf,
			    uint16_t buf_len)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Allocate and intialize inquiry command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_INQUIRY, buf, buf_len);
	if (ret != 0)
		return ret;

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
	if (page) {
		cmd.cdb[1] = 0x01;
		cmd.cdb[2] = page;
	}
	zbc_sg_set_int16(&cmd.cdb[3], buf_len);

	/* Execute the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Test is ZBC/ZAC SAT is working.
 */
static int zbc_scsi_test_sat(struct zbc_device *dev)
{
	size_t bufsz = 512;
	struct zbc_sg_cmd cmd;
	int ret;

	/* Allocate and intialize report zones command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_REPORT_ZONES, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (95h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |       Service Action (00h)                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                        Zone Start LBA                           ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                        Allocation Length                        ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |Partial |Reserved|                 Reporting Options                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_REPORT_ZONES_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_REPORT_ZONES_CDB_SA;
	zbc_sg_set_int64(&cmd.cdb[2], 0);
	zbc_sg_set_int32(&cmd.cdb[10], (unsigned int) bufsz);
	cmd.cdb[14] = ZBC_RZ_RO_PARTIAL;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	if (ret != 0)
		zbc_debug("%s: ZBC/ZAC SAT not supported\n",
			  dev->zbd_filename);
	else
		zbc_debug("%s: ZBC/ZAC SAT supported: treating ATA device as SCSI\n",
			  dev->zbd_filename);

	return ret;
}

/**
 * Get information string from inquiry output.
 */
static inline char *zbc_scsi_str(char *dst, uint8_t *buf, int len)
{
	char *str = (char *) buf;
	int i;

	for (i = len - 1; i >= 0; i--) {
	       if (isalnum(str[i]))
		       break;
	}

	if (i >= 0)
		memcpy(dst, str, i + 1);

	return dst;
}

#define ZBC_SCSI_VID_LEN	8
#define ZBC_SCSI_PID_LEN	16
#define ZBC_SCSI_REV_LEN	4

/**
 * Get information (model, vendor, ...) from a SCSI device.
 */
static int zbc_scsi_classify(struct zbc_device *dev)
{
	uint8_t buf[ZBC_SCSI_INQUIRY_BUF_LEN];
	char vid[ZBC_SCSI_VID_LEN + 1];
	char pid[ZBC_SCSI_PID_LEN + 1];
	char rev[ZBC_SCSI_REV_LEN + 1];
	uint8_t zoned;
	int dev_type;
	int ret;

	/* Get device info */
	ret = zbc_scsi_inquiry(dev, 0, buf, ZBC_SCSI_INQUIRY_BUF_LEN);
	if (ret != 0) {
		zbc_error("%s: zbc_scsi_inquiry failed\n",
			  dev->zbd_filename);
		return ret;
	}

	/*
	 * If this is an ATA drive, try to see if SAT is working.
	 * If SAT is working, treat the disk as SCSI.
	 */
	if (strncmp((char *)&buf[8], "ATA", 3) == 0) {
		ret = zbc_scsi_test_sat(dev);
		if (ret != 0)
			return -ENXIO;
	}

	/* This is a SCSI device */
	dev->zbd_info.zbd_type = ZBC_DT_SCSI;

	/*
	 * Concatenate vendor identification, product identification
	 * and product revision strings.
	 */
	//memset(dev->zbd_info.zbd_vendor_id, 0, ZBC_DEVICE_INFO_LENGTH);
	memset(vid, 0, sizeof(vid));
	memset(pid, 0, sizeof(pid));
	memset(rev, 0, sizeof(rev));
	sprintf(dev->zbd_info.zbd_vendor_id,
		 "%s %s %s",
		 zbc_scsi_str(vid, &buf[8], 8),
		 zbc_scsi_str(pid, &buf[16], 16),
		 zbc_scsi_str(rev, &buf[32], 4));

	/* Now check the device type */
	dev_type = (int)(buf[0] & 0x1f);
	switch (dev_type) {

	case 0x14:
		/* Host-managed device */
		zbc_debug("%s: Host-managed ZBC block device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
		break;

	case 0x00:
		/* Standard block device */
		break;

	default:
		/* Unsupported device */
		zbc_error("%s: Unsupported device type 0x%02X\n",
			  dev->zbd_filename, dev_type);
		return -ENXIO;
	}

	/*
	 * If the device has a standard block device type, the device
	 * may be a host-aware one. So look at the block device characteristics
	 * VPD page (B1h) to be sure. Also check that no weird value is
	 * reported by the zoned field for host-managed devices.
	 */
	memset(buf, 0, sizeof(buf));
	ret = zbc_scsi_inquiry(dev, 0xB1, buf, ZBC_SCSI_VPD_PAGE_B1_LEN);
	if (ret != 0) {
		zbc_error("%s: zbc_scsi_inquiry VPD page 0xB1 failed\n",
			  dev->zbd_filename);
		return ret;
	}

	if ((buf[1] != 0xB1) ||
	    (buf[2] != 0x00) ||
	    (buf[3] != 0x3C)) {
		zbc_error("%s: Invalid zbc_scsi_inquiry VPD page 0xB1 result\n",
			  dev->zbd_filename);
		return -EIO;
	}

	/* FIXME MUTATE support bit is not standard, testing only */
	if (buf[9] & 0x01)
		dev->zbd_info.zbd_flags |= ZBC_MUTATE_SUPPORT;

	zoned = (buf[8] & 0x30) >> 4;
	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED) {
		if (zbc_test_mode(dev) && zoned != 0) {
			zbc_error("%s: Invalid host-managed device ZONED field 0x%02x\n",
				  dev->zbd_filename, zoned);
			return -EIO;
		}
		if (zoned != 0)
			zbc_warning("%s: Invalid host-managed device ZONED field 0x%02x\n",
				    dev->zbd_filename, zoned);
		return 0;
	}

	switch (zoned) {
	case 0x00:
		if (buf[8] & 0x40) {
			/* Zone Domains host-managed device */
			zbc_debug("%s: Zone Domains SCSI block device detected\n",
				  dev->zbd_filename);
			dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
			dev->zbd_info.zbd_flags |= ZBC_ZONE_DOMAINS_SUPPORT;
		} else {
			zbc_debug("%s: Standard SCSI block device detected\n",
				dev->zbd_filename);
			dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
			if (dev->zbd_info.zbd_flags & ZBC_MUTATE_SUPPORT)
				return 0;
			else
				return -ENXIO;
		}
		break;

	case 0x01:
		/* Host aware device */
		zbc_debug("%s: Host-aware ZBC block device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
		break;

	case 0x02:
		/* Device-managed device */
		zbc_debug("%s: Device-managed SCSI block device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_DEVICE_MANAGED;
		return -ENXIO;

	default:
		zbc_debug("%s: Unknown device model 0x%02x\n",
			  dev->zbd_filename, zoned);
		dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
		return -EIO;
	}

	return 0;
}

/**
 * Get a SCSI device zone information.
 */
static int zbc_scsi_do_report_zones(struct zbc_device *dev, uint64_t sector,
				    enum zbc_zone_reporting_options ro,
				    uint64_t *max_lba,
				    struct zbc_zone *zones,
				    unsigned int *nr_zones)
{
	size_t bufsz = ZBC_ZONE_DESCRIPTOR_OFFSET;
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int i, nz = 0, buf_nz;
	struct zbc_sg_cmd cmd;
	uint8_t const *buf;
	int ret;

	if (*nr_zones)
		bufsz += (size_t)*nr_zones * ZBC_ZONE_DESCRIPTOR_LENGTH;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and intialize report zones command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_REPORT_ZONES, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (95h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |       Service Action (00h)                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                        Zone Start LBA                           ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                        Allocation Length                        ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |Partial |Reserved|                 Reporting Options                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_REPORT_ZONES_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_REPORT_ZONES_CDB_SA;
	zbc_sg_set_int64(&cmd.cdb[2], lba);
	zbc_sg_set_int32(&cmd.cdb[10], (unsigned int) bufsz);
	cmd.cdb[14] = ro & 0xbf;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_ZONE_DESCRIPTOR_OFFSET) {
		zbc_error("%s: Not enough report data received (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_ZONE_DESCRIPTOR_OFFSET,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Process output:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * |  0  | (MSB)                                                                 |
	 * |- - -+---               Zone List Length (n - 64)                         ---|
	 * |  3  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * |  4  |              Reserved             |               Same                |
	 * |-----+-----------------------------------------------------------------------|
	 * |  5  |                                                                       |
	 * |- - -+---                        Reserved                                 ---|
	 * |  7  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * |  8  | (MSB)                                                                 |
	 * |- - -+---                      Maximum LBA                                ---|
	 * | 15  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 16  | (MSB)                                                                 |
	 * |- - -+---                        Reserved                                 ---|
	 * | 63  |                                                                 (LSB) |
	 * |=====+=======================================================================|
	 * |     |                       Vendor-Specific Parameters                      |
	 * |=====+=======================================================================|
	 * | 64  | (MSB)                                                                 |
	 * |- - -+---                  Zone Descriptor [first]                        ---|
	 * | 127 |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * |                                    .                                        |
	 * |                                    .                                        |
	 * |                                    .                                        |
	 * |-----+-----------------------------------------------------------------------|
	 * |n-63 |                                                                       |
	 * |- - -+---                   Zone Descriptor [last]                        ---|
	 * | n   |                                                                       |
	 * +=============================================================================+
	 */

	/* Get number of zones in result */
	buf = cmd.out_buf;
	nz = zbc_sg_get_int32(buf) / ZBC_ZONE_DESCRIPTOR_LENGTH;
	if (max_lba)
		*max_lba = zbc_sg_get_int64(&buf[8]);

	if (!zones || !nz)
		goto out;

	/* Get zone info */
	if (nz > *nr_zones)
		nz = *nr_zones;

	buf_nz = (cmd.out_bufsz - ZBC_ZONE_DESCRIPTOR_OFFSET)
		/ ZBC_ZONE_DESCRIPTOR_LENGTH;
	if (nz > buf_nz)
		nz = buf_nz;

	/* Get zone descriptors:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * |  0  |             Reserved              |            Zone type              |
	 * |-----+-----------------------------------------------------------------------|
	 * |  1  |          Zone condition           |    Reserved     |non-seq |  Reset |
	 * |-----+-----------------------------------------------------------------------|
	 * |  2  |                                                                       |
	 * |- - -+---                             Reserved                            ---|
	 * |  7  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * |  8  | (MSB)                                                                 |
	 * |- - -+---                           Zone Length                           ---|
	 * | 15  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 16  | (MSB)                                                                 |
	 * |- - -+---                          Zone Start LBA                         ---|
	 * | 23  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 24  | (MSB)                                                                 |
	 * |- - -+---                         Write Pointer LBA                       ---|
	 * | 31  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 32  |                                                                       |
	 * |- - -+---                             Reserved                            ---|
	 * | 63  |                                                                       |
	 * +=============================================================================+
	 */
	buf += ZBC_ZONE_DESCRIPTOR_OFFSET;
	for (i = 0; i < nz; i++) {

		zones[i].zbz_type = buf[0] & 0x0f;

		zones[i].zbz_attributes = buf[1] & 0x03;
		zones[i].zbz_condition = (buf[1] >> 4) & 0x0f;

		zones[i].zbz_length =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[8]));
		zones[i].zbz_start =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[16]));
		if (zbc_zone_sequential(&zones[i]) || zbc_zone_sobr(&zones[i]))
			zones[i].zbz_write_pointer =
				zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[24]));
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
 * Get a SCSI device zone information.
 */
static int zbc_scsi_report_zones(struct zbc_device *dev, uint64_t sector,
				 enum zbc_zone_reporting_options ro,
				 struct zbc_zone *zones, unsigned int *nr_zones)
{
	return zbc_scsi_do_report_zones(dev, sector, ro,
					NULL, zones, nr_zones);
}

/**
 * Zone(s) operation.
 */
int zbc_scsi_zone_op(struct zbc_device *dev, uint64_t sector,
		     unsigned int count, enum zbc_zone_op op,
		     unsigned int flags)
{
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int cmdid;
	unsigned int cmdcode;
	unsigned int cmdsa;
	struct zbc_sg_cmd cmd;
	int ret;

	switch (op) {
	case ZBC_OP_RESET_ZONE:
		cmdid = ZBC_SG_RESET_ZONE;
		cmdcode = ZBC_SG_RESET_ZONE_CDB_OPCODE;
		cmdsa = ZBC_SG_RESET_ZONE_CDB_SA;
		break;
	case ZBC_OP_OPEN_ZONE:
		cmdid = ZBC_SG_OPEN_ZONE;
		cmdcode = ZBC_SG_OPEN_ZONE_CDB_OPCODE;
		cmdsa = ZBC_SG_OPEN_ZONE_CDB_SA;
		break;
	case ZBC_OP_CLOSE_ZONE:
		cmdid = ZBC_SG_CLOSE_ZONE;
		cmdcode = ZBC_SG_CLOSE_ZONE_CDB_OPCODE;
		cmdsa = ZBC_SG_CLOSE_ZONE_CDB_SA;
		break;
	case ZBC_OP_FINISH_ZONE:
		cmdid = ZBC_SG_FINISH_ZONE;
		cmdcode = ZBC_SG_FINISH_ZONE_CDB_OPCODE;
		cmdsa = ZBC_SG_FINISH_ZONE_CDB_SA;
		break;
	default:
		zbc_error("%s: Invalid operation code 0x%x\n",
			  dev->zbd_filename, op);
		return -EINVAL;
	}

	/* Allocate and intialize zone command */
	ret = zbc_sg_cmd_init(dev, &cmd, cmdid, NULL, 0);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * | 0   |                           Operation Code                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |        Reserved          |               Service Action               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                           Zone ID                               ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                           Reserved                              ---|
	 * | 11  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  | (MSB)                                                                 |
	 * |- - -+---                          Zone count                             ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                           Reserved                           |  All   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                              Control                                  |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = cmdcode;
	cmd.cdb[1] = cmdsa;
	if (flags & ZBC_OP_ALL_ZONES)
		/* Operate on all zones */
		cmd.cdb[14] = 0x01;
	else
		/* Operate on the zone at lba */
		zbc_sg_set_int64(&cmd.cdb[2], lba);

	if (zbc_test_mode(dev))
		zbc_sg_set_int64(&cmd.cdb[2], lba);

	zbc_sg_set_int16(&cmd.cdb[12], count);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Report device zone domain configuration.
 */
static int zbc_scsi_report_domains(struct zbc_device *dev, uint64_t sector,
				   enum zbc_domain_report_options ro,
				   struct zbc_zone_domain *domains,
				   unsigned int nr_domains)
{
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	size_t bufsz = ZBC_RPT_DOMAINS_HEADER_SIZE;
	unsigned int i, nd = 0, sz;
	struct zbc_sg_cmd cmd;
	uint8_t const *buf;
	int ret;

	bufsz += (size_t)nr_domains * ZBC_RPT_DOMAINS_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and intialize REPORT ZONE DOMAINS command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_REPORT_ZONE_DOMAINS, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * | 0   |                           Operation Code (95h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |           Service Action (07h)             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                         Realm Start LBA                         ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                        Allocation Length                        ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |     Reserved    |                 Reporting Options                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                              Control                                  |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_REPORT_ZONE_DOMAINS_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_REPORT_ZONE_DOMAINS_CDB_SA;
	zbc_sg_set_int64(&cmd.cdb[2], lba);
	zbc_sg_set_int32(&cmd.cdb[10], (unsigned int)bufsz);
	cmd.cdb[14] = ro & 0x3f;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_RPT_DOMAINS_HEADER_SIZE) {
		zbc_error("%s: Not enough REPORT ZONE DOMAINS data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_RPT_DOMAINS_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get the number of domain descriptors in result */
	buf = cmd.out_buf;
	nd = buf[9];

	if (!domains || !nd)
		goto out;

	/* Get the number of domain descriptors to fill */
	if (nd < nr_domains)
		nr_domains = nd;

	sz = zbc_sg_get_int32(&buf[4]);
	bufsz = sz < cmd.out_bufsz ? sz : cmd.out_bufsz;
	bufsz -= ZBC_RPT_DOMAINS_HEADER_SIZE;
	bufsz /= ZBC_RPT_DOMAINS_RECORD_SIZE;
	if (nr_domains > bufsz)
		nr_domains = bufsz;

	/* Get zone domain descriptors */
	buf += ZBC_RPT_DOMAINS_HEADER_SIZE;
	for (i = 0; i < nr_domains; i++) {
		domains[i].zbm_id = buf[0];

		domains[i].zbm_nr_zones = zbc_sg_get_int64(&buf[16]);
		domains[i].zbm_start_lba =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[24]));
		domains[i].zbm_end_lba =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[32]));

		if (!(buf[41] & 0x02)) { /*  VALID DOMAIN ZONE TYPE */
			zbc_error("%s: Zone type not valid for domain %u, flags=0x%x\n",
				  dev->zbd_filename, buf[0], buf[41]);
			/*
			 * We could have done REPORT ZONES on domain start LBA to
			 * get the type, but if the bit is not set, then the type
			 * is likely to be variable across the domain and we don't
			 * support this functionality.
			 */
			ret = -EIO;
			goto out;
		}
		domains[i].zbm_type = buf[40];

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
static int zbc_scsi_report_realms(struct zbc_device *dev, uint64_t sector,
				  enum zbc_realm_report_options ro,
				  struct zbc_zone_realm *realms,
				  unsigned int *nr_realms)
{
	struct zbc_zone_domain *domains = NULL, *d;
	struct zbc_realm_item *ri;
	uint8_t const *buf, *ptr;
	uint64_t zone_size, lba = zbc_dev_sect2lba(dev, sector);
#ifdef ZBC_NEW_RPT_REALMS
	uint64_t next;
#endif
	size_t bufsz = ZBC_RPT_REALMS_HEADER_SIZE;
	unsigned int i, nr = 0, desc_len;
	struct zbc_sg_cmd cmd;
	int ret, j, nr_domains;

	/*
	 * Always get zone domains first. Allocate the buffer for
	 * ZBC_NR_ZONE_TYPES domains since we will be only able to
	 * process as many.
	 */
	nr_domains = ZBC_NR_ZONE_TYPES;
	domains = calloc(nr_domains, sizeof(struct zbc_zone_domain));
	if (!domains)
		return -ENOMEM;

	nr_domains = zbc_scsi_report_domains(dev, 0LL, ZBC_RZD_RO_ALL, domains,
					     nr_domains);
	if (nr_domains > ZBC_NR_ZONE_TYPES) {
		zbc_warning("%s: Device has %i domains, only %u are supported\n",
			    dev->zbd_filename, nr_domains, ZBC_NR_ZONE_TYPES);

		nr_domains = ZBC_NR_ZONE_TYPES;
	}
	if (nr_domains < 0) {
		free(domains);
		return nr_domains;
	}

	if (*nr_realms)
		bufsz += (size_t)*nr_realms * ZBC_RPT_REALMS_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and intialize REPORT REALMS command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_REPORT_REALMS, NULL, bufsz);
	if (ret != 0) {
		free(domains);
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * | 0   |                           Operation Code (95h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |           Service Action (06h)             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                         Realm Start LBA                         ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                        Allocation Length                        ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |     Reserved    |                 Reporting Options                   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                               Control                                 |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_REPORT_REALMS_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_REPORT_REALMS_CDB_SA;
	zbc_sg_set_int64(&cmd.cdb[2], lba);
	zbc_sg_set_int32(&cmd.cdb[10], (unsigned int)bufsz);
	cmd.cdb[14] = ro & 0x3f;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_RPT_REALMS_HEADER_SIZE) {
		zbc_error("%s: Not enough REPORT REALMS data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_RPT_REALMS_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get number of realm descriptors from the header */
	buf = cmd.out_buf;
	nr = zbc_sg_get_int32(&buf[0]);

	if (!realms || !nr)
		goto out;

	/* Get the number of realm descriptors to fill */
	if (nr > *nr_realms)
		nr = *nr_realms;

#ifdef ZBC_NEW_RPT_REALMS
	desc_len = zbc_sg_get_int32(&buf[4]);
	if (!desc_len)
		goto oldrealms; /* The field is reserved pre ZDr4, so it has to be 0 */
	next = zbc_sg_get_int64(&buf[8]);
	if (next) {
		zbc_error("%s: NEXT REALM LOCATOR is not yet supported",
			  dev->zbd_filename);
		ret = -ENXIO; /* FIXME handle */
		goto out;
	}

	bufsz = (cmd.out_bufsz - ZBC_RPT_REALMS_HEADER_SIZE) / desc_len;
	if (nr > bufsz)
		nr = bufsz;

	/* Get zone realm descriptors */
	buf += ZBC_RPT_REALMS_HEADER_SIZE;
	for (i = 0; i < nr; i++, realms++) {
		realms->zbr_number = zbc_sg_get_int32(buf);
		realms->zbr_restr = zbc_sg_get_int16(&buf[4]);
		realms->zbr_dom_id = buf[7];
		if (realms->zbr_dom_id < ZBC_NR_ZONE_TYPES)
			realms->zbr_type = domains[realms->zbr_dom_id].zbm_type;
		realms->zbr_nr_domains = nr_domains;
		ptr = buf + ZBC_RPT_REALMS_DESC_OFFSET;
		/* FIXME don't use nr_domains, use desc_len to limit iteration */
		for (j = 0; j < nr_domains; j++) {
			ri = &realms->zbr_ri[j];
			ri->zbi_end_lba =
					zbc_dev_lba2sect(dev, zbc_sg_get_int64(ptr + 8));
			if (ri->zbi_end_lba) {
				realms->zbr_actv_flags |= (1 << j);
				d = &domains[j];
				ri->zbi_dom_id = j;
				ri->zbi_type = d->zbm_type;
				ri->zbi_start_lba =
					zbc_dev_lba2sect(dev, zbc_sg_get_int64(ptr));
				if (d->zbm_nr_zones) {
					zone_size = (d->zbm_end_lba + 1 - d->zbm_start_lba) /
						    d->zbm_nr_zones;
				} else {
					zone_size = 0;
				}
				if (zone_size) {
					ri->zbi_length = (ri->zbi_end_lba + 1 - ri->zbi_start_lba) /
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

	bufsz = (cmd.out_bufsz - ZBC_RPT_REALMS_HEADER_SIZE) / desc_len;
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
		realms->zbr_number = zbc_sg_get_int16(&buf[2]);
		realms->zbr_nr_domains = nr_domains;
		ptr = buf + ZBC_RPT_REALMS_DESC_OFFSET;
		for (j = 0; j < nr_domains; j++) {
			if (realms->zbr_actv_flags & (1 << j)) {
				d = &domains[j];
				ri = &realms->zbr_ri[j];
				ri->zbi_dom_id = j;
				ri->zbi_type = d->zbm_type;
				ri->zbi_start_lba =
					zbc_dev_lba2sect(dev, zbc_sg_get_int64(ptr));
				ri->zbi_end_lba =
					zbc_dev_lba2sect(dev, zbc_sg_get_int64(ptr + 8));
				if (d->zbm_nr_zones) {
					zone_size = (d->zbm_end_lba + 1 - d->zbm_start_lba) /
						    d->zbm_nr_zones;
				} else {
					zone_size = 0;
				}
				if (zone_size) {
					ri->zbi_length = (ri->zbi_end_lba + 1 - ri->zbi_start_lba) /
							 zone_size;
				}
			}
			ptr += ZBC_RPT_REALMS_ITEM_SIZE;
		}

		buf += desc_len;
	}

out:
	if (domains)
		free(domains);

	/* Return the number of realm descriptors */
	*nr_realms = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 *  Perform Zone Query / Activate operation using the 16-byte CDB.
 */
static int zbc_scsi_zone_activate16(struct zbc_device *dev, bool zsrc, bool all,
				    uint64_t zone_start_id, bool query,
				    uint32_t nr_zones, unsigned int domain_id,
				    struct zbc_actv_res *actv_recs,
				    unsigned int *nr_actv_recs)
{
	size_t bufsz = ZBC_ACTV_RES_HEADER_SIZE;
	unsigned int i, nr = 0;
	struct zbc_sg_cmd cmd;
	uint16_t stat;
	uint8_t const *buf;
	int ret;

	if (*nr_actv_recs)
		bufsz += (size_t)*nr_actv_recs * ZBC_ACTV_RES_RECORD_SIZE;
	if (*nr_actv_recs > (uint16_t)(-1)) {
		zbc_error("%s: # of activation result records %u is too high\n",
			dev->zbd_filename, *nr_actv_recs);
		return -EINVAL;
	}
	if (zone_start_id > 0xffffffffffffLL) {
		zbc_error("%s: # Zone start ID %"PRIu64" is too high\n",
			dev->zbd_filename, zone_start_id);
		return -EINVAL;
	}

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and intialize ZONE QUERY/ACTIVATE(16) command */
	ret = zbc_sg_cmd_init(dev, &cmd,
			      query ? ZBC_SG_ZONE_QUERY_16 :
				      ZBC_SG_ZONE_ACTIVATE_16, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+=======================================================================|
	 * | 0   |                        Operation Code (95h)                           |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |         Reserved         |          Service Action (08h/09h)          |
	 * |-----+--------+--------------------------------------------------------------|
	 * | 2   |  All   |  ZSRC  |                    Domain ID                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   | (MSB)                                                                 |
	 * |- - -+---                        Zone Start ID                            ---|
	 * | 8   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   | (MSB)                                                                 |
	 * |- - -+---                      Allocated Length                           ---|
	 * | 12  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  | (MSB)                                                                 |
	 * |- - -+---                       Number Of Zones                           ---|
	 * | 14  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                              Control                                  |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_ZONE_QUERY_ACTV_16_CDB_OPCODE;
	if (query)
		cmd.cdb[1] = ZBC_SG_ZONE_QUERY_16_CDB_SA;
	else
		cmd.cdb[1] = ZBC_SG_ZONE_ACTIVATE_16_CDB_SA;
	cmd.cdb[2] = domain_id & 0x3f; /* Domain ID to activate */
	if (all)
		cmd.cdb[2] |= 0x80; /* All */
	if (zsrc) {
		cmd.cdb[2] |= 0x40; /* ZSRC */
		zbc_sg_set_int16(&cmd.cdb[13], (uint16_t)nr_zones);
	}
	zbc_sg_set_int48(&cmd.cdb[3], zone_start_id);
	zbc_sg_set_int32(&cmd.cdb[9], (uint32_t)bufsz);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_ACTV_RES_HEADER_SIZE) {
		zbc_error("%s: Not enough report data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_ACTV_RES_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	buf = cmd.out_buf;

	if (zbc_log_level >= ZBC_LOG_DEBUG) {
		size_t sz = ZBC_ACTV_RES_HEADER_SIZE + zbc_sg_get_int32(buf);
		if (sz > cmd.out_bufsz)
			sz = cmd.out_bufsz;
		zbc_debug("%s: %s REPLY (%zd/%zd B):\n",
				(query ? "QUERY" : "ACTIVATE"),
				dev->zbd_filename,
				sz, cmd.out_bufsz);
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
		dev->zbd_errno.err_za = stat;
		if (stat & 0x4000) /* ZIWUP valid */
			dev->zbd_errno.err_cbf = zbc_sg_get_int48(&buf[24]);
		zbc_warning("%s: Zones %s activated {ERR=0x%04x CBF=%"PRIu64" (%svalid)}\n",
			    dev->zbd_filename, query ? "will not be" : "not",
			    stat, dev->zbd_errno.err_cbf,
			    ((stat & 0x4000) ? "" : "in"));
		ret = -EIO;

		/* There still might be descriptors returned, try to read them */
	}

	/* Get number of activation records in result */
	if (!actv_recs) {
		nr = zbc_sg_get_int32(buf) / ZBC_ACTV_RES_RECORD_SIZE;
		goto out;
	}
	nr = zbc_sg_get_int32(&buf[4]) / ZBC_ACTV_RES_RECORD_SIZE;
	if (!nr)
		goto out;

	/*
	 * Only get as many activation results records
	 * as the allocated buffer allows.
	 */
	if (nr > *nr_actv_recs)
		nr = *nr_actv_recs;

	bufsz = (cmd.out_bufsz - ZBC_ACTV_RES_HEADER_SIZE) /
		ZBC_ACTV_RES_RECORD_SIZE;
	if (nr > bufsz)
		nr = bufsz;

	/* Get the activation results records */
	buf += ZBC_ACTV_RES_HEADER_SIZE;
	for (i = 0; i < nr; i++) {
		actv_recs[i].zbe_type = buf[0] & 0x0f;
		actv_recs[i].zbe_condition = (buf[1] >> 4) & 0x0f;
		actv_recs[i].zbe_domain = buf[2];
		actv_recs[i].zbe_nr_zones = zbc_sg_get_int64(&buf[8]);
		actv_recs[i].zbe_start_zone =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[16]));

		buf += ZBC_ACTV_RES_RECORD_SIZE;
	}

out:
	/* Return the number of descriptors */
	*nr_actv_recs = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 *  Perform Zone Query / Activate operation using the 32-byte CDB.
 */
static int zbc_scsi_zone_activate32(struct zbc_device *dev, bool zsrc, bool all,
				    uint64_t zone_start_id, bool query,
				    uint32_t nr_zones, uint32_t domain_id,
				    struct zbc_actv_res *actv_recs,
				    uint32_t *nr_actv_recs)
{
	size_t bufsz = ZBC_ACTV_RES_HEADER_SIZE;
	unsigned int i, nr = 0;
	struct zbc_sg_cmd cmd;
	uint8_t const *buf;
	uint16_t stat;
	int ret;

	if (*nr_actv_recs)
		bufsz += (size_t)*nr_actv_recs * ZBC_ACTV_RES_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and intialize ZONE QUERY/ACTIVATE(32) command */
	ret = zbc_sg_cmd_init(dev, &cmd,
			      query ? ZBC_SG_ZONE_QUERY_32 :
				      ZBC_SG_ZONE_ACTIVATE_32, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                  Operation Code (7Fh FIXME TBD)                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |                              Control                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |                                                                       |
	 * |- - -+                             Reserved                                  |
	 * | 6   |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 7   |                    Additional CDB Length (18h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   | (MSB)                                                                 |
	 * |-----+---     Service Action (F800h for Activate, F810h for Query)        ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |  All   |  ZSRC  |                      Reserved                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                               Domain ID                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  | (MSB)                                                                 |
	 * |-----+---                         Starting Zone ID                        ---|
	 * | 19  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 20  |                                                                       |
	 * |-----+---                        Number Of Zones                          ---|
	 * | 23  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 24  |                                                                       |
	 * |-----+---                           Reserved                              ---|
	 * | 27  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 28  | (MSB)                                                                 |
	 * |- - -+---                       Allocated Length                          ---|
	 * | 31  |                                                                 (LSB) |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_ZONE_QUERY_ACTV_32_CDB_OPCODE;
	cmd.cdb[7] = 0x18;
	zbc_sg_set_int16(&cmd.cdb[8], query ? ZBC_SG_ZONE_QUERY_32_CDB_SA :
					      ZBC_SG_ZONE_ACTIVATE_32_CDB_SA);

	if (zsrc) {
		cmd.cdb[10] |= 0x40; /* ZSRC */
		zbc_sg_set_int32(&cmd.cdb[20], nr_zones);
	}

	if (all)
		cmd.cdb[10] |= 0x80; /* All */

	cmd.cdb[11] = domain_id; /* Domain ID to activate */

	zbc_sg_set_int64(&cmd.cdb[12], zone_start_id);
	zbc_sg_set_int32(&cmd.cdb[28], bufsz);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_ACTV_RES_HEADER_SIZE) {
		zbc_error("%s: Not enough report data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_ACTV_RES_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	buf = cmd.out_buf;

	if (zbc_log_level >= ZBC_LOG_DEBUG) {
		size_t sz = ZBC_ACTV_RES_HEADER_SIZE + zbc_sg_get_int32(buf);
		if (sz > cmd.out_bufsz)
			sz = cmd.out_bufsz;
		zbc_debug("%s: %s REPLY (%zd/%zd B):\n",
				(query ? "QUERY" : "ACTIVATE"),
				dev->zbd_filename,
				sz, cmd.out_bufsz);
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
		dev->zbd_errno.err_za = stat;
		if (stat & 0x4000) /* ZIWUP valid */
			dev->zbd_errno.err_cbf = zbc_sg_get_int48(&buf[24]);
		zbc_warning("%s: Zones %s activated {ERR=0x%04x CBF=%"PRIu64" (%svalid)}\n",
			    dev->zbd_filename,
			    query ? "will not be" : "not",
			    dev->zbd_errno.err_za, dev->zbd_errno.err_cbf,
			    ((dev->zbd_errno.err_za & 0x4000) ? "" : "in"));
		ret = -EIO;

		/* There still might be descriptors returned, try to read them */
	}

	/* Get number of activation records in result */
	if (!actv_recs) {
		nr = zbc_sg_get_int32(buf) / ZBC_ACTV_RES_RECORD_SIZE;
		goto out;
	}
	nr = zbc_sg_get_int32(&buf[4]) / ZBC_ACTV_RES_RECORD_SIZE;
	if (!nr)
		goto out;

	/*
	 * Only get as many activation results records
	 * as the allocated buffer allows.
	 */
	if (nr > *nr_actv_recs)
		nr = *nr_actv_recs;

	bufsz = (cmd.out_bufsz - ZBC_ACTV_RES_HEADER_SIZE) /
		ZBC_ACTV_RES_RECORD_SIZE;
	if (nr > bufsz)
		nr = bufsz;

	/* Get the activation results records */
	buf += ZBC_ACTV_RES_HEADER_SIZE;
	for (i = 0; i < nr; i++) {
		actv_recs[i].zbe_type = buf[0] & 0x0f;
		actv_recs[i].zbe_condition = (buf[1] >> 4) & 0x0f;
		actv_recs[i].zbe_domain = buf[2];
		actv_recs[i].zbe_nr_zones = zbc_sg_get_int64(&buf[8]);
		actv_recs[i].zbe_start_zone =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[16]));

		buf += ZBC_ACTV_RES_RECORD_SIZE;
	}

out:
	/* Return the number of recordrs */
	*nr_actv_recs = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);


	return ret;
}

static int zbc_scsi_zone_query_activate(struct zbc_device *dev, bool zsrc, bool all,
					bool use_32_byte_cdb, bool query,
					uint64_t lba, uint32_t nr_zones,
					unsigned int domain_id,
					struct zbc_actv_res *actv_recs,
					unsigned int *nr_actv_recs)
{
	if (all)
		zsrc = false;

	return use_32_byte_cdb ?
	       zbc_scsi_zone_activate32(dev, zsrc, all, lba, query, nr_zones,
					domain_id, actv_recs, nr_actv_recs) :
	       zbc_scsi_zone_activate16(dev, zsrc, all, lba, query, nr_zones,
					domain_id, actv_recs, nr_actv_recs);
}

/**
 * Read or set values in one of device mode pages.
 */
static int zbc_scsi_get_set_mode(struct zbc_device *dev, uint32_t pg,
				 uint32_t subpg, uint8_t *buf,
				 uint32_t buf_len, bool set, uint32_t *pg_len)
{
	struct zbc_sg_cmd cmd;
	uint8_t *data = NULL;
	uint32_t len, max_bufsz, bufsz = buf_len + ZBC_MODE_PAGE_OFFSET;
	int ret;

	if (pg_len)
		*pg_len = 0;

	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

	data = calloc(1, bufsz);
	if (!data)
		return -ENOMEM;

	if (!set) {
		memset(data, 0, bufsz);

		/* MODE SENSE 10 */
		ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_MODE_SENSE,
				      data, bufsz);
	} else {
		memset(data, 0, ZBC_MODE_PAGE_OFFSET);
		memcpy(&data[ZBC_MODE_PAGE_OFFSET], buf, buf_len);

		/* MODE SELECT 10 */
		ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_MODE_SELECT,
				      data, bufsz);
	}
	if (ret != 0)
		goto err;

	/* Fill command CDB */
	cmd.cdb[1] = 0x10; /* PF */
	if (!set) {
		cmd.cdb[0] = MODE_SENSE_10;
		cmd.cdb[2] = pg & 0x3f;
		cmd.cdb[3] = subpg;
	} else {
		cmd.cdb[0] = MODE_SELECT_10;
		data[8] = pg & 0x3f;
		data[8] |= 0x40;
		data[9] = subpg;
		zbc_sg_set_int16(&data[10], buf_len);
	}
	zbc_sg_set_int16(&cmd.cdb[7], bufsz);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (!set) {
		len = zbc_sg_get_int16(&data[0]);
		if (len > buf_len)
			len = buf_len;
		memcpy(buf, &data[ZBC_MODE_PAGE_OFFSET], len);
		if (pg_len)
			*pg_len = len;
	}
out:
	zbc_sg_cmd_destroy(&cmd);

err:
	if (data)
		free(data);

	return ret;
}

/**
 * Read or set Zone Domains configuration parameters.
 */
static int zbc_scsi_dev_control(struct zbc_device *dev,
				struct zbc_zd_dev_control *ctl, bool set)
{
	int ret;
	unsigned int pg_len;
	bool update = false;
	uint8_t mode_page[ZBC_SCSI_MODE_PG_SIZE];

	ret = zbc_scsi_get_set_mode(dev,
				    ZBC_ZONE_DOM_MODE_PG,
				    ZBC_ZONE_DOM_MODE_SUBPG, mode_page,
				    ZBC_SCSI_MODE_PG_SIZE, false, &pg_len);
	if (ret) {
		zbc_error("%s: Can't read Zone Domains mode page\n",
			  dev->zbd_filename);
		return ret;
	}
	if (pg_len < ZBC_SCSI_MIN_MODE_PG_SIZE) {
		zbc_error("%s: Zone Domains mode page too short, %iB\n",
			  dev->zbd_filename, pg_len);
		return -EIO;
	}

	if (!set) {
		memset(ctl, 0, sizeof(*ctl));
		ctl->zbt_nr_zones = zbc_sg_get_int32(&mode_page[0]);
		ctl->zbt_urswrz = mode_page[6];
		ctl->zbt_max_activate = zbc_sg_get_int16(&mode_page[12]);
		return ret;
	}

	if (ctl->zbt_nr_zones != 0xffffffff) {
		zbc_sg_set_int32(&mode_page[0], ctl->zbt_nr_zones);
		update = true;
	}
	if (ctl->zbt_urswrz != 0xff) {
		mode_page[6] = ctl->zbt_urswrz;
		update = true;
	}
	if (ctl->zbt_max_activate != 0xffff) {
		zbc_sg_set_int16(&mode_page[12], ctl->zbt_max_activate);
		update = true;
	}

	if (!update)
		return ret;

	ret = zbc_scsi_get_set_mode(dev,
				    ZBC_ZONE_DOM_MODE_PG,
				    ZBC_ZONE_DOM_MODE_SUBPG,
				    mode_page, pg_len,
				    true, NULL);
	if (ret)
		zbc_error("%s: Can't update Zone Domains mode page\n",
			  dev->zbd_filename);
	else
		zbc_debug("%s: MODE SELECT fsnoz=%u urswrz=%u max_activate=%u\n",
				dev->zbd_filename, ctl->zbt_nr_zones,
				ctl->zbt_urswrz, ctl->zbt_max_activate);

	return ret;
}

/**
 * Get supported device mutations.
 */
static int zbc_scsi_report_mutations(struct zbc_device *dev,
				     struct zbc_supported_mutation *sm,
				     unsigned int *nr_sm_recs)
{
	size_t bufsz = ZBC_MUTATE_RPT_HEADER_SIZE;
	unsigned int i, nrecs = 0, buf_nrecs;
	struct zbc_sg_cmd cmd;
	uint8_t const *buf;
	int ret;

	if (*nr_sm_recs)
		bufsz += ((size_t)*nr_sm_recs) * ZBC_MUTATE_RPT_RECORD_SIZE;

	bufsz = zbc_sg_align_bufsz(dev, bufsz);

	/* Allocate and intialize report mutations command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_REPORT_MUTATIONS, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (95h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |       Service Action (04h)                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |                                                                       |
	 * |- - -+---                             Reserved                            ---|
	 * | 9   |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                        Allocation Length                        ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |                                Reserved                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                                 Control                               |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_REPORT_MUTATIONS_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_REPORT_MUTATIONS_CDB_SA;
	zbc_sg_set_int32(&cmd.cdb[10], (unsigned int) bufsz);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_MUTATE_RPT_HEADER_SIZE) {
		zbc_error("%s: Not enough report data received (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_MUTATE_RPT_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get number of records in result */
	buf = cmd.out_buf;
	nrecs = zbc_sg_get_int32(buf);

	if (!*nr_sm_recs)
		goto done;
	if (!sm || !nrecs)
		goto out;

	/* Calculate the number of records to output */
	if (nrecs > *nr_sm_recs)
		nrecs = *nr_sm_recs;

	buf_nrecs = (cmd.out_bufsz - ZBC_MUTATE_RPT_HEADER_SIZE)
		/ ZBC_MUTATE_RPT_RECORD_SIZE;
	if (nrecs > buf_nrecs)
		nrecs = buf_nrecs;

	/* Output the supported mutation records */
	buf += ZBC_MUTATE_RPT_HEADER_SIZE;
	for (i = 0; i < nrecs; i++) {
		sm[i].zbs_mt = buf[0] & 0x0f;
		sm[i].zbs_opt.nz = zbc_sg_get_int32(&buf[4]);

		buf += ZBC_MUTATE_RPT_RECORD_SIZE;
	}

out:
	/* Return number of supported mutations */
	*nr_sm_recs = nrecs;
done:
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}
/**
 * Mutate a device to a different type, PMR <-> SMR <-> DH-SMR.
 */
static int zbc_scsi_mutate(struct zbc_device *dev, enum zbc_mutation_target mt,
			   union zbc_mutation_opt opt)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Allocate and intialize MUTATE command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_MUTATE, NULL, 0);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (94h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |         Reserved         |             Service Action (05h)           |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |                          Mutation target type                         |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                                Reserved                               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   | (MSB)                                                                 |
	 * |- - -+---                     Mutation option (model)                     ---|
	 * | 7   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                                                                       |
	 * |- - -+---                             Reserved                            ---|
	 * | 14  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                                Control                                |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_MUTATE_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_MUTATE_CDB_SA;
	cmd.cdb[2] = mt;
	zbc_sg_set_int32(&cmd.cdb[4], (unsigned int)opt.nz);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get a device capacity information (total sectors & sector sizes).
 */
static int zbc_scsi_get_capacity(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int logical_per_physical;
	unsigned int nr_zones = 0;
	uint64_t max_lba;
	int ret;

	/* READ CAPACITY 16 */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_READ_CAPACITY,
			      NULL, ZBC_SCSI_READ_CAPACITY_BUF_LEN);
	if (ret != 0)
		return ret;

	/* Fill command CDB */
	cmd.cdb[0] = ZBC_SG_READ_CAPACITY_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_READ_CAPACITY_CDB_SA;
	zbc_sg_set_int32(&cmd.cdb[10], ZBC_SCSI_READ_CAPACITY_BUF_LEN);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	/* Logical block size */
	dev->zbd_info.zbd_lblock_size = zbc_sg_get_int32(&cmd.out_buf[8]);
	if (dev->zbd_info.zbd_lblock_size == 0) {
		zbc_error("%s: invalid logical sector size\n",
			  dev->zbd_filename);
		ret = -EIO;
		goto out;
	}

	logical_per_physical = 1 << cmd.out_buf[13] & 0x0f;
	max_lba = zbc_sg_get_int64(&cmd.out_buf[0]);

	/* Get maximum command size */
	zbc_sg_get_max_cmd_blocks(dev);

	if (zbc_dev_is_zoned(dev)) {

		/* Check RC_BASIS field */
		switch ((cmd.out_buf[12] & 0x30) >> 4) {

		case 0x00:
			/*
			 * The capacity represents only the space used by
			 * conventional zones at the beginning of the device.
			 * To get the entire device capacity, we need to get
			 * the last LBA of the last zone of the device.
			 */
			ret = zbc_scsi_do_report_zones(dev, 0,
						       ZBC_RZ_RO_ALL | ZBC_RZ_RO_PARTIAL,
						       &max_lba,
						       NULL, &nr_zones);
			if (ret != 0)
				goto out;

			break;

		case 0x01:
			/* The device max LBA was reported */
			break;

		default:
			zbc_error("%s: invalid RC_BASIS field in READ CAPACITY result\n",
				  dev->zbd_filename);
			ret = -EIO;
			goto out;

		}

	}

	/* Set the drive capacity using the reported max LBA */
	dev->zbd_info.zbd_lblocks = max_lba + 1;

	if (!dev->zbd_info.zbd_lblocks) {
		zbc_error("%s: invalid capacity (logical blocks)\n",
			  dev->zbd_filename);
		ret = -EIO;
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

	return ret;
}

/**
 * Get zoned block device characteristics VPD page information
 * (Maximum or optimum number of open zones).
 */
int zbc_scsi_get_zbd_characteristics(struct zbc_device *dev)
{
	struct zbc_device_info *di = &dev->zbd_info;
	uint8_t buf[ZBC_SCSI_VPD_PAGE_B6_LEN];
	uint32_t val;
	int ret;

	if (!zbc_dev_is_zoned(dev))
		return 0;

	ret = zbc_scsi_inquiry(dev, 0xB6, buf, ZBC_SCSI_VPD_PAGE_B6_LEN);
	if (ret != 0) {
		zbc_error("%s: zbc_scsi_inquiry VPD page 0xB6 failed\n",
			  dev->zbd_filename);
		return ret;
	}

	/* URSWRZ, Zone Domains support */
	di->zbd_flags |= (buf[4] & 0x01) ? ZBC_UNRESTRICTED_READ : 0;
	di->zbd_flags |= (buf[4] & 0x02) ? ZBC_ZONE_DOMAINS_SUPPORT : 0;
	if (di->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) {

		di->zbd_model = ZBC_DM_HOST_MANAGED;

		/* Check what Zone Domains features are supported */
		di->zbd_flags |= (buf[4] & 0x04) ? ZBC_MAXACT_SET_SUPPORT : 0;
		di->zbd_flags |= (buf[4] & 0x08) ? ZBC_NOZSRC_SUPPORT : 0;
		di->zbd_flags |= (buf[4] & 0x10) ? ZBC_URSWRZ_SET_SUPPORT : 0;
		di->zbd_flags |= (buf[4] & 0x20) ? ZBC_REPORT_REALMS_SUPPORT : 0;
		di->zbd_flags |= (buf[4] & 0x80) ? ZBC_ZA_CONTROL_SUPPORT : 0;

		/* Check what zone types are supported. FIXME layout TBD for SCSI */
		di->zbd_flags |= (buf[10] & 0x01) ? ZBC_CONV_ZONE_SUPPORT : 0;
		di->zbd_flags |= (buf[10] & 0x02) ? ZBC_SEQ_PREF_ZONE_SUPPORT : 0;
		di->zbd_flags |= (buf[10] & 0x04) ? ZBC_SEQ_REQ_ZONE_SUPPORT : 0;
		di->zbd_flags |= (buf[10] & 0x08) ? ZBC_SOBR_ZONE_SUPPORT : 0;
		di->zbd_flags |= (buf[10] & 0x10) ? ZBC_GAP_ZONE_SUPPORT : 0;
	}

	/* Maximum number of zones for resource management */
	if (di->zbd_model == ZBC_DM_HOST_AWARE) {

		val = zbc_sg_get_int32(&buf[8]);
		if (!val) {
			/* Handle this case as "not reported" */
			zbc_warning("%s: invalid optimal number of open "
				    "sequential write preferred zones\n",
				    dev->zbd_filename);
			val = ZBC_NOT_REPORTED;
		}
		di->zbd_opt_nr_open_seq_pref = val;

		val = zbc_sg_get_int32(&buf[12]);
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

		val = zbc_sg_get_int32(&buf[16]);
		if (!val) {
			/* Handle this case as "no limit" */
			zbc_warning("%s: invalid maximum number of open "
				    "sequential write required zones\n",
				    dev->zbd_filename);
			val = ZBC_NO_LIMIT;
		}
		di->zbd_max_nr_open_seq_req = val;

	}

	di->zbd_max_activation = zbc_sg_get_int16(&buf[20]);

	return 0;
}

/**
 * Get a device information (capacity & sector sizes).
 */
static int zbc_scsi_get_dev_info(struct zbc_device *dev)
{
	int ret;

	/* Make sure the device is ready */
	ret = zbc_sg_test_unit_ready(dev);
	if (ret != 0)
		return ret;

	/* Get device model */
	ret = zbc_scsi_classify(dev);
	if (ret != 0)
		return ret;

	/* Get capacity information */
	ret = zbc_scsi_get_capacity(dev);
	if (ret != 0)
		return ret;

	/* Get zoned block device characteristics */
	ret = zbc_scsi_get_zbd_characteristics(dev);
	if (ret != 0)
		return ret;

	return 0;
}

/**
 * Open a device.
 */
static int zbc_scsi_open(const char *filename,
			 int flags, struct zbc_device **pdev)
{
	struct zbc_device *dev;
	struct stat st;
	int fd, ret;

	zbc_debug("%s: ########## Trying SCSI driver ##########\n",
		  filename);

	/* Open the device file */
	fd = open(filename, flags & ZBC_O_MODE_MASK);
	if (fd < 0) {
		ret = -errno;
		zbc_error("%s: Open device file failed %d (%s)\n",
			  filename,
			  errno, strerror(errno));
		goto out;
	}

	/* Check device */
	if (fstat(fd, &st) != 0) {
		ret = -errno;
		zbc_error("%s: Stat device file failed %d (%s)\n",
			  filename,
			  errno, strerror(errno));
		goto out;
	}

	if (!S_ISCHR(st.st_mode) &&
	    !S_ISBLK(st.st_mode)) {
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

	ret = zbc_scsi_get_dev_info(dev);
	if (ret != 0)
		goto out_free_filename;

	*pdev = dev;

	zbc_debug("%s: ########## SCSI driver succeeded ##########\n",
		  filename);

	return 0;

out_free_filename:
	free(dev->zbd_filename);

out_free_dev:
	free(dev);

out:
	if (fd >= 0)
		close(fd);

	zbc_debug("%s: ########## SCSI driver failed %d ##########\n",
		  filename,
		  ret);

	return ret;
}

/**
 * Close a device.
 */
static int zbc_scsi_close(struct zbc_device *dev)
{

	if (close(dev->zbd_fd))
		return -errno;

	free(dev->zbd_filename);
	free(dev);

	return 0;
}

/**
 * Read from a ZBC device
 */
ssize_t zbc_scsi_pread(struct zbc_device *dev, void *buf,
		       size_t count, uint64_t offset)
{
	size_t sz = count << 9;
	struct zbc_sg_cmd cmd;
	ssize_t ret;

	/* READ 16 */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_READ, buf, sz);
	if (ret != 0)
		return ret;

	/* Fill command CDB */
	cmd.cdb[0] = ZBC_SG_READ_CDB_OPCODE;
	cmd.cdb[1] = 0x10;
	zbc_sg_set_int64(&cmd.cdb[2], zbc_dev_sect2lba(dev, offset));
	zbc_sg_set_int32(&cmd.cdb[10], zbc_dev_sect2lba(dev, count));

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret == 0)
		ret = (sz - cmd.io_hdr.resid) >> 9;

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Write to a ZBC device
 */
ssize_t zbc_scsi_pwrite(struct zbc_device *dev, const void *buf,
			size_t count, uint64_t offset)
{
	size_t sz = count << 9;
	struct zbc_sg_cmd cmd;
	ssize_t ret;

	/* WRITE 16 */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_WRITE, buf, sz);
	if (ret != 0)
		return ret;

	/* Fill command CDB */
	cmd.cdb[0] = ZBC_SG_WRITE_CDB_OPCODE;
	cmd.cdb[1] = 0x10;
	zbc_sg_set_int64(&cmd.cdb[2], zbc_dev_sect2lba(dev, offset));
	zbc_sg_set_int32(&cmd.cdb[10], zbc_dev_sect2lba(dev, count));

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret == 0)
		ret = (sz - cmd.io_hdr.resid) >> 9;

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Receive ZBD statistic counters from the device.
 */
static int zbc_scsi_get_stats(struct zbc_device * dev,
			      struct zbc_zoned_blk_dev_stats *stats)
{
	uint8_t *bufptr;
	struct zbc_sg_cmd cmd;
	unsigned long long val;
	int ret, i;
	uint16_t size, pc;
	uint8_t buf[4096];

	memset(stats, 0, sizeof(*stats));

	/* Allocate and intialize RECEIVE DIAGNOSTIC RESULTS command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_RECEIVE_DIAG_RESULTS,
			      buf, sizeof(buf));
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                         Operation Code (1Ch)                          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |                          Reserved                            |  PCV   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |                              Page Code                                |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                                                                       |
	 * |- - -+                         Allocation Length                             |
	 * | 4   |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 5   |                              Control                                  |
	 * +-----+-----------------------------------------------------------------------+
	 */
	cmd.cdb[0] = ZBC_SG_RECEIVE_DIAG_RES_CDB_OPCODE;
	cmd.cdb[1] = 0x01; /* PCV=1 : page code valid */
	cmd.cdb[2] = ZBC_SG_ZBD_LOG_STATS;
	zbc_sg_set_int16(&cmd.cdb[3], sizeof(buf));

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	ret = buf[0] & ~0x40;
	if (ret != ZBC_SG_ZBD_LOG_STATS) {
		zbc_error("%s: unsupported diagnostic page 0x%02x\n",
			  dev->zbd_filename, ret);
		ret = -EIO;
		goto out;
	}
	ret = 0;
	if (!(buf[0] & 0x40) || buf[1] != 0x01) {
		zbc_error("%s: invalid diagnostic subpage 0x%02x (SPF=0x%02x)\n",
			  dev->zbd_filename, buf[1], buf[0] & 0x40);
		ret = -EIO;
		goto out;
	}
	size = zbc_sg_get_int16(&buf[2]);
	bufptr = &buf[4];

	/*
	 * Navigate through the list of parameters
	 * and populate the supplied stats structure.
	 */
	for (i = 0; i < ZBC_NR_STAT_PARAMS; i++) {
		if (size < ZBC_LOG_PARAM_RECORD_SIZE) {
			zbc_error("%s: not enough (%i/%i) log parameters returned\n",
				  dev->zbd_filename, i, ZBC_NR_STAT_PARAMS);
			ret = -EIO;
			goto out;
		}

		pc = zbc_sg_get_int16(bufptr);
		if (bufptr[3] != 8) {
			zbc_error("%s: bad ZBC log parameter length %i, 8 expected\n",
				  dev->zbd_filename, bufptr[3]);
			ret = -EIO;
			goto out;
		}

		val = zbc_sg_get_int64(&bufptr[4]);
		switch (pc) {
		case 0x00:
			stats->max_open_zones = val; break;
		case 0x01:
			stats->max_exp_open_seq_zones = val; break;
		case 0x02:
			stats->max_imp_open_seq_zones = val; break;
		case 0x03:
			stats->min_empty_zones = val; break;
		case 0x04:
			stats->max_non_seq_zones = val; break;
		case 0x05:
			stats->zones_emptied = val; break;
		case 0x06:
			stats->subopt_write_cmds = val; break;
		case 0x07:
			stats->cmds_above_opt_lim = val; break;
		case 0x08:
			stats->failed_exp_opens = val; break;
		case 0x09:
			stats->read_rule_fails = val; break;
		case 0x0a:
			stats->write_rule_fails = val; break;
		case 0x0b:
			stats->max_imp_open_sobr_zones = val; break;
		default:
			zbc_error("%s: Bad ZBD log parameter code 0x%02x\n",
				  dev->zbd_filename, pc);
			ret = -EIO;
			goto out;
		}

		bufptr += ZBC_LOG_PARAM_RECORD_SIZE;
		size -= ZBC_LOG_PARAM_RECORD_SIZE;
	}

out:
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Flush a ZBC device cache.
 */
int zbc_scsi_flush(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* SYNCHRONIZE CACHE 16 */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_SYNC_CACHE, NULL, 0);
	if (ret != 0)
		return ret;

	/* Fill command CDB (immediate flush) */
	cmd.cdb[0] = ZBC_SG_SYNC_CACHE_CDB_OPCODE;
	cmd.cdb[1] = 0x02;
	zbc_sg_set_int64(&cmd.cdb[2], 0);
	zbc_sg_set_int32(&cmd.cdb[10], 0);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * ZBC SCSI device driver definition.
 */
struct zbc_drv zbc_scsi_drv = {
	.flag			= ZBC_O_DRV_SCSI,
	.zbd_open		= zbc_scsi_open,
	.zbd_close		= zbc_scsi_close,
	.zbd_dev_control	= zbc_scsi_dev_control,
	.zbd_pread		= zbc_scsi_pread,
	.zbd_pwrite		= zbc_scsi_pwrite,
	.zbd_flush		= zbc_scsi_flush,
	.zbd_report_zones	= zbc_scsi_report_zones,
	.zbd_zone_op		= zbc_scsi_zone_op,
	.zbd_report_domains	= zbc_scsi_report_domains,
	.zbd_report_realms	= zbc_scsi_report_realms,
	.zbd_zone_query_actv	= zbc_scsi_zone_query_activate,
	.zbd_get_stats		= zbc_scsi_get_stats,
	.zbd_report_mutations	= zbc_scsi_report_mutations,
	.zbd_mutate		= zbc_scsi_mutate,
};

