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

/*
 * MEDIA REPORT ouput header size.
 */
#define ZBC_RANGE_HEADER_SIZE		64

/*
 * MEDIA REPORT output conversion descriptor size.
 */
#define ZBC_RANGE_RECORD_SIZE		128

/**
 * SCSI commands reply length.
 */
#define ZBC_SCSI_INQUIRY_BUF_LEN	96
#define ZBC_SCSI_VPD_PAGE_B1_LEN	64
#define ZBC_SCSI_VPD_PAGE_B6_LEN	64
#define ZBC_SCSI_READ_CAPACITY_BUF_LEN	32

/**
 * Media Conversion Results header and descriptor sizes.
 */
#define ZBC_CONV_RES_HEADER_SIZE	32
#define ZBC_CONV_RES_RECORD_SIZE	16

/**
 * ZONE PROVISIONING mode page size
 */
#define ZBC_SCSI_MODE_PG_SIZE		256

/**
 * ZONE PROVISIONING mode page minimum size
 */
#define ZBC_SCSI_MIN_MODE_PG_SIZE	8

/**
 * Data offset from the beginning of MODE SENSE/SELSECT data buffer
 * to the first byte of the actual page.
 */
#define ZBC_MODE_PAGE_OFFSET		12
/**
 * ZONE PROVISIONING mode page and subpage numbers.
 * FIXME The values below are in vendor-specific range and will change
 */
#define ZBC_ZONE_PROV_MODE_PG		0x3d
#define ZBC_ZONE_PROV_MODE_SUBPG	0x08

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
	cmd.cdb[14] = ZBC_RO_PARTIAL;

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
		zbc_debug("%s: Standard SCSI block device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
		return -ENXIO;

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

	case 0x03:
		/* Realm host-managed device */
		zbc_debug("%s: Realm host-managed SCSI block device detected\n",
			  dev->zbd_filename);
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
		dev->zbd_info.zbd_flags |= ZBC_MEDIA_CVT_SUPPORT;
		break;

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
				    enum zbc_reporting_options ro,
				    uint64_t *max_lba,
				    struct zbc_zone *zones,
				    unsigned int *nr_zones)
{
	size_t bufsz = ZBC_ZONE_DESCRIPTOR_OFFSET;
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	unsigned int i, nz = 0, buf_nz;
	struct zbc_sg_cmd cmd;
	size_t max_bufsz;
	uint8_t *buf;
	int ret;

	if (*nr_zones)
		bufsz += (size_t)*nr_zones * ZBC_ZONE_DESCRIPTOR_LENGTH;

	/* For in kernel ATA translation: align to 512 B */
	bufsz = (bufsz + 511) & ~511;
	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

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
	buf = (uint8_t *) cmd.out_buf;
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
		if (zbc_zone_sequential(&zones[i]) || zbc_zone_conv_wp(&zones[i]))
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
				 enum zbc_reporting_options ro,
				 struct zbc_zone *zones, unsigned int *nr_zones)
{
	return zbc_scsi_do_report_zones(dev, sector, ro,
					NULL, zones, nr_zones);
}

/**
 * Zone(s) operation.
 */
int zbc_scsi_zone_op(struct zbc_device *dev, uint64_t sector,
		     enum zbc_zone_op op, unsigned int flags)
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
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code                              |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |               Service Action               |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                        Zone ID                                  ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  | (MSB)                                                                 |
	 * |- - -+---                        Reserved                                 ---|
	 * | 13  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 14  |               Reserved                                       |  All   |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                           Control                                     |
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

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Report device media conversion configuration.
 */
static int zbc_scsi_media_report(struct zbc_device *dev,
				 struct zbc_cvt_range *ranges,
				 unsigned int *nr_ranges)
{
	size_t bufsz = ZBC_RANGE_HEADER_SIZE;
	unsigned int i, nr = 0;
	struct zbc_sg_cmd cmd;
	size_t max_bufsz;
	uint8_t *buf;
	int ret;

	if (*nr_ranges)
		bufsz += (size_t)*nr_ranges * ZBC_RANGE_RECORD_SIZE;

	/* For in kernel ATA translation: align to 512 B */
	bufsz = (bufsz + 511) & ~511;
	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

	/* Allocate and intialize MEDIA REPORT command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_MEDIA_REPORT, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (95h)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |       Service Action (01h) FIXME TBD       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |                         Reporting Options                             |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                             Reserved                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   | (MSB)                                                                 |
	 * |- - -+---                       Allocation Length                         ---|
	 * | 7   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                                                                       |
	 * |-----+---                          Reserved                               ---|
	 * | 14  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                             Control                                   |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_MEDIA_REPORT_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_MEDIA_REPORT_CDB_SA;
	zbc_sg_set_int32(&cmd.cdb[10], (unsigned int)bufsz);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_RANGE_HEADER_SIZE) {
		zbc_error("%s: Not enough report data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_RANGE_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	/* Get number of range descriptors in result */
	buf = (uint8_t *)cmd.out_buf;
	/* FIXME header format TBD */
	nr = zbc_sg_get_int32(buf) / ZBC_RANGE_RECORD_SIZE;

	if (!ranges || !nr)
		goto out;

	/* Get the number of renge descriptors to fill */
	if (nr > *nr_ranges)
		nr = *nr_ranges;

	bufsz = (cmd.out_bufsz - ZBC_RANGE_HEADER_SIZE) /
		ZBC_RANGE_RECORD_SIZE;
	if (nr > bufsz)
		nr = bufsz;

	/* Get conversion range descriptors */
	buf += ZBC_RANGE_HEADER_SIZE;
	for (i = 0; i < nr; i++) {
		ranges[i].zbr_type = buf[0] & 0x0f;
		ranges[i].zbr_convertible = buf[1];

		ranges[i].zbr_number = zbc_sg_get_int16(&buf[2]);
		ranges[i].zbr_keep_out = zbc_sg_get_int16(&buf[4]);

		ranges[i].zbr_conv_start =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[16]));
		ranges[i].zbr_conv_length = zbc_sg_get_int32(&buf[24]);
		ranges[i].zbr_seq_start =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[32]));
		ranges[i].zbr_seq_length = zbc_sg_get_int32(&buf[40]);

		buf += ZBC_RANGE_RECORD_SIZE;
	}

out:
	/* Return number of range descriptors */
	*nr_ranges = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Convert one or several realms from one type to another.
 */
static int zbc_scsi_convert_realms(struct zbc_device *dev,
				   uint64_t start_realm, uint32_t count,
				   enum zbc_zone_type new_type, int fg)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* Allocate and intialize CONVERT REALMS command */
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_CONVERT_REALMS, NULL, 0);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                        Operation Code (94h)                           |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |  FGND  |    Reserved     |         Service Action (06h) FIXME TBD     |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |           New Zone Type           | Rservd |        Options           |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   |                             Reserved                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 4   | (MSB)                                                                 |
	 * |- - -+---                       Starting Realm                            ---|
	 * | 5   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 6   | (MSB)                                                                 |
	 * |- - -+---                         Realm Count                             ---|
	 * | 7   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 8   |                                                                       |
	 * |- - -|---                          Reserved                               ---|
	 * | 14  |                                                                       |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                              Control                                  |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_CONVERT_REALMS_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_CONVERT_REALMS_CDB_SA;
	/* Fill in the starting realm number and the realm count */
	zbc_sg_set_int16(&cmd.cdb[4], start_realm);
	zbc_sg_set_int16(&cmd.cdb[6], (unsigned short)count);
	/* Init the new zone type and FOREGROUND flag */
	cmd.cdb[2] = new_type << 4;
	if (fg)
		cmd.cdb[1] |= 0x80;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 *  Perform Media Query / Convert operation using 16-byte CDB.
 */
static int zbc_scsi_media_convert16(struct zbc_device *dev, bool all,
				    uint64_t start_zone_lba, bool query,
				    uint32_t nr_zones, bool to_cmr,
				    bool fg, struct zbc_conv_rec *conv_recs,
				    unsigned int *nr_conv_recs)
{
	size_t bufsz = ZBC_CONV_RES_HEADER_SIZE;
	unsigned int i, nr = 0;
	struct zbc_sg_cmd cmd;
	size_t max_bufsz;
	uint8_t *buf;
	int ret;

	if (nr_zones > (uint16_t)(-1)) {
		zbc_error("%s: # of zones to convert %u is too high\n",
			  dev->zbd_filename, nr_zones);
		return -EINVAL;
	}

	if (*nr_conv_recs) {
		if (*nr_conv_recs > (uint16_t)(-1)) {
			zbc_error("%s: # of convert records %u is too high\n",
				dev->zbd_filename, *nr_conv_recs);
			return -EINVAL;
		}
		bufsz += (size_t)*nr_conv_recs * ZBC_CONV_RES_RECORD_SIZE;
	} else
		bufsz += ZBC_CONV_RES_RECORD_SIZE;

	/* For in kernel ATA translation: align to 512 B */
	bufsz = (bufsz + 511) & ~511;
	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

	/* Allocate and intialize MEDIA QUERY/CONVERT (16) command */
	ret = zbc_sg_cmd_init(dev, &cmd,
			      query ? ZBC_SG_MEDIA_QUERY_16 :
				      ZBC_SG_MEDIA_CONVERT_16, NULL, bufsz);
	if (ret != 0)
		return ret;

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                        Operation Code (4Ah)                           |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |  All   |  DIR   | Rsrvd  |          Service Action (01h/02h)          |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   |  FGND  |                   Reserved                          |  ZSRC  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 3   | (MSB)                                                                 |
	 * |- - -+---                  Starting Zone Locator (LBA)                    ---|
	 * | 10  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  | (MSB)                                                                 |
	 * |- - -+---                        Record Count                             ---|
	 * | 12  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 13  | (MSB)                                                                 |
	 * |- - -+---                       Number Of Zones                           ---|
	 * | 14  |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 15  |                              Control                                  |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_MEDIA_QUERY_CVT_16_CDB_OPCODE;
	if (query)
		cmd.cdb[1] = ZBC_SG_MEDIA_QUERY_16_CDB_SA;
	else
		cmd.cdb[1] = ZBC_SG_MEDIA_CONVERT_16_CDB_SA;
	if (to_cmr)
		cmd.cdb[1] |= 0x40; /* DIR */
	if (all)
		cmd.cdb[1] |= 0x80; /* All */
	if (nr_zones)
		cmd.cdb[2] |= 0x01; /* ZSRC */
	if (fg)
		cmd.cdb[2] |= 0x80; /* FGND */
	zbc_sg_set_int64(&cmd.cdb[2], start_zone_lba);
	if (*nr_conv_recs)
		zbc_sg_set_int16(&cmd.cdb[10], (uint16_t)(*nr_conv_recs));
	else
		zbc_sg_set_int16(&cmd.cdb[10], 1);

	if (nr_zones)
		zbc_sg_set_int16(&cmd.cdb[12], (uint16_t)nr_zones);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_CONV_RES_HEADER_SIZE) {
		zbc_error("%s: Not enough report data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_CONV_RES_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	buf = (uint8_t *)cmd.out_buf;

	/*
	 * FIXME analyze error bits and choose an error code
	 * to return if they are set. For now, just check
	 * CONVERTED bit.
	 */
	if ((buf[5] & 0x80) == 0) {
		zbc_warning("%s: Media %s converted\n",
			    dev->zbd_filename,
			    query ? "will not be" : "not");
		ret = -EIO;
		/* Not bailing here, gonna try to get the descriptors */
	}

	/* Get number of descriptors in result */
	nr = zbc_sg_get_int32(buf) / ZBC_CONV_RES_RECORD_SIZE;

	if (!conv_recs || !nr)
		goto out;

	/*
	 * Only get as many conversion descriptors as
	 * the allocated buffer allows
	 */
	if (nr > *nr_conv_recs)
		nr = *nr_conv_recs;

	bufsz = (cmd.out_bufsz - ZBC_CONV_RES_HEADER_SIZE) /
		ZBC_CONV_RES_RECORD_SIZE;
	if (nr > bufsz)
		nr = bufsz;

	/* Get the conversion descriptors */
	buf += ZBC_CONV_RES_HEADER_SIZE;
	for (i = 0; i < nr; i++) {
		conv_recs[i].zbe_type = buf[0] & 0x0f;
		conv_recs[i].zbe_condition = (buf[1] >> 4) & 0x0f;
		conv_recs[i].zbe_nr_zones = zbc_sg_get_int32(&buf[4]);
		conv_recs[i].zbe_start_lba =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[8]));

		buf += ZBC_CONV_RES_RECORD_SIZE;
	}

out:
	/* Return the number of descriptors */
	*nr_conv_recs = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 *  Perform Media Query / Convert operation using 32-byte CDB.
 */
static int zbc_scsi_media_convert32(struct zbc_device *dev, bool all,
				    uint64_t start_zone_lba, bool query,
				    uint32_t nr_zones, bool to_cmr,
				    bool fg, struct zbc_conv_rec *conv_recs,
				    unsigned int *nr_conv_recs)
{
	size_t bufsz = ZBC_CONV_RES_HEADER_SIZE;
	unsigned int i, nr = 0;
	struct zbc_sg_cmd cmd;
	size_t max_bufsz;
	uint8_t *buf;
	int ret;

	if (*nr_conv_recs)
		bufsz += (size_t)*nr_conv_recs * ZBC_CONV_RES_RECORD_SIZE;
	else
		bufsz += ZBC_CONV_RES_RECORD_SIZE;

	/* For in kernel ATA translation: align to 512 B */
	bufsz = (bufsz + 511) & ~511;
	max_bufsz = dev->zbd_info.zbd_max_rw_sectors << 9;
	if (bufsz > max_bufsz)
		bufsz = max_bufsz;

	/* Allocate and intialize MEDIA QUERY/CONVERT (32) command */
	ret = zbc_sg_cmd_init(dev, &cmd,
			      query ? ZBC_SG_MEDIA_QUERY_32 :
				      ZBC_SG_MEDIA_CONVERT_32, NULL, bufsz);
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
	 * |-----+---     Service Action (F800h for Convert, F810h for Query)         ---|
	 * | 9   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 10  |  All   |  FGND  |  DIR   |  ZSRC  |          Reserved                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 11  |                             Reserved                                  |
	 * |-----+-----------------------------------------------------------------------|
	 * | 12  | (MSB)                                                                 |
	 * |-----+---                   Starting Zone Locator (LBA)                   ---|
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
	 * |- - -+---                          Record Count                           ---|
	 * | 31  |                                                                 (LSB) |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_MEDIA_QUERY_CVT_32_CDB_OPCODE;
	cmd.cdb[7] = 0x18;
	zbc_sg_set_int16(&cmd.cdb[8], query ? ZBC_SG_MEDIA_QUERY_32_CDB_SA :
					      ZBC_SG_MEDIA_CONVERT_32_CDB_SA);
	cmd.cdb[10] = to_cmr ? 0x20 : 0; /* DIR */
	if (nr_zones)
		cmd.cdb[10] |= 0x10; /* ZSRC */
	if (fg)
		cmd.cdb[10] |= 0x40; /* FGND */
	if (all)
		cmd.cdb[10] |= 0x80; /* All */
	zbc_sg_set_int64(&cmd.cdb[12], start_zone_lba);
	if (nr_zones)
	zbc_sg_set_int32(&cmd.cdb[20], nr_zones);
	if (*nr_conv_recs)
		zbc_sg_set_int32(&cmd.cdb[28], *nr_conv_recs);
	else
		zbc_sg_set_int32(&cmd.cdb[28], 1);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if (cmd.out_bufsz < ZBC_CONV_RES_HEADER_SIZE) {
		zbc_error("%s: Not enough report data received"
			  " (need at least %d B, got %zu B)\n",
			  dev->zbd_filename,
			  ZBC_CONV_RES_HEADER_SIZE,
			  cmd.out_bufsz);
		ret = -EIO;
		goto out;
	}

	buf = (uint8_t *)cmd.out_buf;

	/*
	 * FIXME analyze error bits and choose an error code
	 * to return if they are set. For now, just check
	 * CONVERTED bit.
	 */
	if ((buf[5] & 0x80) == 0) {
		zbc_warning("%s: Media %s converted\n",
			    dev->zbd_filename,
			    query ? "will not be" : "not");
		ret = -EIO;
		/* Not bailing here, gonna try to get the descriptors */
	}

	/* Get number of descriptors in result */
	nr = zbc_sg_get_int32(buf) / ZBC_CONV_RES_RECORD_SIZE;

	if (!conv_recs || !nr)
		goto out;

	/*
	 * Only get as many conversion descriptors
	 * as the allocated buffer allows.
	 */
	if (nr > *nr_conv_recs)
		nr = *nr_conv_recs;

	bufsz = (cmd.out_bufsz - ZBC_CONV_RES_HEADER_SIZE) /
		ZBC_CONV_RES_RECORD_SIZE;
	if (nr > bufsz)
		nr = bufsz;

	/* Get the conversion descriptors */
	buf += ZBC_CONV_RES_HEADER_SIZE;
	for (i = 0; i < nr; i++) {
		conv_recs[i].zbe_type = buf[0] & 0x0f;
		conv_recs[i].zbe_condition = (buf[1] >> 4) & 0x0f;
		conv_recs[i].zbe_nr_zones = zbc_sg_get_int32(&buf[4]);
		conv_recs[i].zbe_start_lba =
			zbc_dev_lba2sect(dev, zbc_sg_get_int64(&buf[8]));

		buf += ZBC_CONV_RES_RECORD_SIZE;
	}

out:
	/* Return the number of descriptors */
	*nr_conv_recs = nr;

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);


	return ret;
}

static int zbc_scsi_media_query_convert(struct zbc_device *dev, bool all,
					bool use_32_byte_cdb, bool query,
					uint64_t lba, uint32_t nr_zones,
					bool to_cmr, bool fg,
				  struct zbc_conv_rec *conv_recs,
				  uint32_t *nr_conv_recs)
{
	return use_32_byte_cdb ?
	       zbc_scsi_media_convert32(dev, all, lba, query, nr_zones, to_cmr,
					fg, conv_recs, nr_conv_recs) :
	       zbc_scsi_media_convert16(dev, all, lba, query, nr_zones, to_cmr,
					fg, conv_recs, nr_conv_recs);
}

/**
 * Get a device capacity information (total sectors & sector sizes).
 */
static int zbc_scsi_get_capacity(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	struct zbc_zone *zones = NULL;
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
			ret = zbc_scsi_do_report_zones(dev, 0, ZBC_RO_ALL|ZBC_RO_PARTIAL,
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

	if (zones)
		free(zones);

	return ret;
}

/**
 * Get zoned block device characteristics VPD page information
 * (Maximum or optimum number of open zones).
 */
int zbc_scsi_get_zbd_characteristics(struct zbc_device *dev)
{
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

	/* URSWRZ, Media Convert support, FC, CWPCE and CWPCS flags */
	dev->zbd_info.zbd_flags |= (buf[4] & 0x01) ? ZBC_UNRESTRICTED_READ : 0;
	dev->zbd_info.zbd_flags |= (buf[4] & 0x02) ? ZBC_MEDIA_CVT_SUPPORT : 0;
	dev->zbd_info.zbd_flags |= (buf[4] & 0x04) ? ZBC_FC_SUPPORT : 0;
	dev->zbd_info.zbd_flags |= (buf[4] & 0x08) ? ZBC_CONV_WP_CHECK : 0; /* FIXME shoud be in MODE PAGE */
	dev->zbd_info.zbd_flags |= (buf[4] & 0x10) ? ZBC_CONV_WP_CHECK_SUPPORT : 0;

	/* Maximum number of zones for resource management */
	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_AWARE) {

		val = zbc_sg_get_int32(&buf[8]);
		if (!val) {
			/* Handle this case as "not reported" */
			zbc_warning("%s: invalid optimal number of open "
				    "sequential write preferred zones\n",
				    dev->zbd_filename);
			val = ZBC_NOT_REPORTED;
		}
		dev->zbd_info.zbd_opt_nr_open_seq_pref = val;

		val = zbc_sg_get_int32(&buf[12]);
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

		val = zbc_sg_get_int32(&buf[16]);
		if (!val) {
			/* Handle this case as "no limit" */
			zbc_warning("%s: invalid maximum number of open "
				    "sequential write required zones\n",
				    dev->zbd_filename);
			val = ZBC_NO_LIMIT;
		}
		dev->zbd_info.zbd_max_nr_open_seq_req = val;

	}

	dev->zbd_info.zbd_max_conversion = zbc_sg_get_int16(&buf[20]);

	dev->zbd_info.zbd_max_conversion = zbc_sg_get_int16(&buf[20]);

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
 * Read or set values in one of device mode pages.
 */
static int zbc_scsi_get_set_mode(struct zbc_device *dev, uint32_t pg,
				 uint32_t subpg, uint8_t *buf,
				 uint32_t buf_len, bool set, uint32_t *pg_len)
{
	struct zbc_sg_cmd cmd;
	uint8_t *data = NULL;
	uint32_t len, bufsz = buf_len + ZBC_MODE_PAGE_OFFSET, max_bufsz;
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
 * Read or set DH-SMR configuration parameters.
 */
static int zbc_scsi_dev_control(struct zbc_device *dev,
				struct zbc_zp_dev_control *ctl, bool set)
{
	int ret;
	unsigned int pg_len;
	bool update = false;
	uint8_t mode_page[ZBC_SCSI_MODE_PG_SIZE];

	ret = zbc_scsi_get_set_mode(dev,
				    ZBC_ZONE_PROV_MODE_PG,
				    ZBC_ZONE_PROV_MODE_SUBPG, mode_page,
				    ZBC_SCSI_MODE_PG_SIZE, false, &pg_len);
	if (ret) {
		zbc_error("%s: Can't read Zone Provisioning mode page\n",
			  dev->zbd_filename);
		return ret;
	}
	if (pg_len < ZBC_SCSI_MIN_MODE_PG_SIZE) {
		zbc_error("%s: Zone Provisioning mode page too short, %iB\n",
			  dev->zbd_filename, pg_len);
		return -EINVAL;
	}

	if (!set) {
		memset(ctl, 0, sizeof(*ctl));
		ctl->zbm_nr_zones = zbc_sg_get_int32(&mode_page[0]);
		ctl->zbm_smr_zone_type = mode_page[4];
		ctl->zbm_cmr_wp_check = mode_page[6];
		return ret;
	}

	if (ctl->zbm_nr_zones != 0xffffffff) {
		zbc_sg_set_int32(&mode_page[0], ctl->zbm_nr_zones);
		update = true;
	}
	if (ctl->zbm_smr_zone_type != 0xff) {
		mode_page[4] = ctl->zbm_smr_zone_type;
		update = true;
	}
	if (ctl->zbm_cmr_wp_check != 0xff) {
		mode_page[6] = ctl->zbm_cmr_wp_check;
		update = true;
	}

	if (!update)
		return ret;

	ret = zbc_scsi_get_set_mode(dev,
				    ZBC_ZONE_PROV_MODE_PG,
				    ZBC_ZONE_PROV_MODE_SUBPG,
				    mode_page, pg_len,
				    true, NULL);
	if (ret)
		zbc_error("%s: Can't update Zone Provisioning mode page\n",
			  dev->zbd_filename);

	return ret;
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
	ret = zbc_sg_cmd_init(dev, &cmd, ZBC_SG_WRITE, (uint8_t *)buf, sz);
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
	.zbd_media_report	= zbc_scsi_media_report,
	.zbd_convert_realms	= zbc_scsi_convert_realms,
	.zbd_media_query_cvt	= zbc_scsi_media_query_convert,
};

