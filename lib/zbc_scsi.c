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
 * SCSI commands reply length.
 */
#define ZBC_SCSI_INQUIRY_BUF_LEN	96
#define ZBC_SCSI_VPD_PAGE_B1_LEN	64
#define ZBC_SCSI_VPD_PAGE_B6_LEN	64
#define ZBC_SCSI_READ_CAPACITY_BUF_LEN	32

/**
 * ZBC Device types.
 */
#define ZBC_DEV_TYPE_STANDARD		0x00
#define ZBC_DEV_TYPE_HOST_MANAGED	0x14

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
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_INQUIRY, buf, buf_len);
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
 * Get information (model, vendor, ...) from a SCSI device.
 */
static int zbc_scsi_classify(struct zbc_device *dev)
{
	uint8_t buf[ZBC_SCSI_INQUIRY_BUF_LEN];
	uint8_t zoned;
	int dev_type;
	int n, ret;

	/* Get device info */
	ret = zbc_scsi_inquiry(dev, 0, buf, ZBC_SCSI_INQUIRY_BUF_LEN);
	if (ret != 0) {
		zbc_error("zbc_scsi_inquiry failed\n");
		return ret;
	}

	/* Make sure we are not dealing with an ATA device */
	if (strncmp((char *)&buf[8], "ATA", 3) == 0)
		return -ENXIO;

	/* This is a SCSI device */
	dev->zbd_info.zbd_type = ZBC_DT_SCSI;

	/* Vendor identification */
	n = zbc_sg_strcpy(&dev->zbd_info.zbd_vendor_id[0],
			  (char *)&buf[8], 8);

	/* Product identification */
	n += zbc_sg_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			   (char *)&buf[16], 16);

	/* Product revision */
	n += zbc_sg_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			   (char *)&buf[32], 4);

	/* Now check the device type */
	dev_type = (int)(buf[0] & 0x1f);
	switch (dev_type) {

	case ZBC_DEV_TYPE_HOST_MANAGED:
		/* Host-managed device */
		zbc_debug("Host-managed ZBC block device detected\n");
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
		break;

	case ZBC_DEV_TYPE_STANDARD:
		break;

	default:
		/* Unsupported device */
		return -ENXIO;
	}

	/*
	 * If we got ZBC_DEV_TYPE_STANDARD, the device may be a
	 * host-aware one. So look at the block device characteristics
	 * VPD page (B1h) to be sure. Also check that no weird value is
	 * reported by the zoned field for host-managed devices.
	 */
	memset(buf, 0, sizeof(buf));
	ret = zbc_scsi_inquiry(dev, 0xB1, buf, ZBC_SCSI_VPD_PAGE_B1_LEN);
	if (ret != 0) {
		zbc_error("zbc_scsi_inquiry VPD page 0xB1 failed\n");
		return ret;
	}

	if ((buf[1] != 0xB1) ||
	    (buf[2] != 0x00) ||
	    (buf[3] != 0x3C)) {
		zbc_error("Invalid zbc_scsi_inquiry VPD page 0xB1 result\n");
		return -EIO;
	}

	zoned = (buf[8] & 0x30) >> 4;
	if (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED) {
		if (zoned != 0) {
			zbc_error("Invalid host-managed device ZONED field 0x%02x\n",
				  zoned);
			return -EIO;
		}
		return 0;
	}

	switch (zoned) {

	case 0x00:
		zbc_debug("Standard SCSI block device detected\n");
		dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
		return -ENXIO;

	case 0x01:
		/* Host aware device */
		zbc_debug("Host-aware ZBC block device detected\n");
		dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
		break;

	case 0x02:
		/* Device-managed device */
		zbc_debug("Device-managed SCSI block device detected\n");
		dev->zbd_info.zbd_model = ZBC_DM_DEVICE_MANAGED;
		return -ENXIO;

	default:
		zbc_debug("Unknown device type\n");
		dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
		return -ENXIO;
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
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_REPORT_ZONES, NULL, bufsz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

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
		zbc_error("Not enough data received "
			  "(need at least %d B, got %zu B)\n",
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
		if (zbc_zone_sequential(&zones[i]))
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
		zbc_error("Invalid operation code 0x%x\n", op);
		return -EINVAL;
	}

	/* Allocate and intialize zone command */
	ret = zbc_sg_cmd_init(&cmd, cmdid, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

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
 * Configure zones of a "emulated" ZBC device
 */
static int zbc_scsi_set_zones(struct zbc_device *dev,
			      uint64_t conv_sz, uint64_t zone_sz)
{
	uint64_t conv_lba = zbc_dev_sect2lba(dev, conv_sz);
	uint64_t zone_lba = zbc_dev_sect2lba(dev, zone_sz);
	struct zbc_sg_cmd cmd;
	int ret;

	/* Allocate and intialize set zone command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SET_ZONES, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (9Fh)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |       Service Action (15h)                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---             Conventional Zone Sise (LBA)                        ---|
	 * | 8   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   | (MSB)                                                                 |
	 * |- - -+---                   Zone Sise (LBA)                               ---|
	 * | 15  |                                                                 (LSB) |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_SET_ZONES_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_SET_ZONES_CDB_SA;
	zbc_sg_set_bytes(&cmd.cdb[2], &conv_lba, 7);
	zbc_sg_set_bytes(&cmd.cdb[9], &zone_lba, 7);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Change the value of a zone write pointer ("emulated" ZBC devices only).
 */
static int zbc_scsi_set_write_pointer(struct zbc_device *dev,
				      uint64_t sector, uint64_t wp_sector)
{
	uint64_t lba = zbc_dev_sect2lba(dev, sector);
	uint64_t wp_lba = zbc_dev_sect2lba(dev, wp_sector);
	struct zbc_sg_cmd cmd;
	int ret;

	/* Allocate and intialize set zone command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SET_WRITE_POINTER, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB:
	 * +=============================================================================+
	 * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
	 * |Byte |        |        |        |        |        |        |        |        |
	 * |=====+==========================+============================================|
	 * | 0   |                           Operation Code (9Fh)                        |
	 * |-----+-----------------------------------------------------------------------|
	 * | 1   |      Reserved            |       Service Action (16h)                 |
	 * |-----+-----------------------------------------------------------------------|
	 * | 2   | (MSB)                                                                 |
	 * |- - -+---                   Start LBA                                     ---|
	 * | 8   |                                                                 (LSB) |
	 * |-----+-----------------------------------------------------------------------|
	 * | 9   | (MSB)                                                                 |
	 * |- - -+---               Write pointer LBA                                 ---|
	 * | 15  |                                                                 (LSB) |
	 * +=============================================================================+
	 */
	cmd.cdb[0] = ZBC_SG_SET_WRITE_POINTER_CDB_OPCODE;
	cmd.cdb[1] = ZBC_SG_SET_WRITE_POINTER_CDB_SA;
	zbc_sg_set_bytes(&cmd.cdb[2], &lba, 7);
	zbc_sg_set_bytes(&cmd.cdb[9], &wp_lba, 7);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
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
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_READ_CAPACITY,
			      NULL, ZBC_SCSI_READ_CAPACITY_BUF_LEN);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

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
		 * conventional zones at the beginning of the device. To get
		 * the entire device capacity, we need to get the last LBA
		 * of the last zone of the device.
		 */
		ret = zbc_scsi_do_report_zones(dev, 0, ZBC_RO_ALL, &max_lba,
					       NULL, &nr_zones);
		if (ret != 0)
			goto out;

		/* Set the drive capacity to the reported max LBA */
		dev->zbd_info.zbd_lblocks = max_lba + 1;

		break;

	case 0x01:

		/* The device last LBA was reported */
		dev->zbd_info.zbd_lblocks =
			zbc_sg_get_int64(&cmd.out_buf[0]) + 1;

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
 * Get zoned block device characteristics VPD page information
 * (Maximum or optimum number of open zones).
 */
int zbc_scsi_get_zbd_characteristics(struct zbc_device *dev)
{
	uint8_t buf[ZBC_SCSI_VPD_PAGE_B6_LEN];
	int ret;

	ret = zbc_scsi_inquiry(dev, 0xB6, buf, ZBC_SCSI_VPD_PAGE_B6_LEN);
	if (ret != 0) {
		zbc_error("zbc_scsi_inquiry VPD page 0xB6 failed\n");
		return ret;
	}

	/* URSWRZ (unrestricted read in sequential write required zone) flag */
	dev->zbd_info.zbd_flags |= (buf[4] & 0x01) ? ZBC_UNRESTRICTED_READ : 0;

	/* Resource of handling zones */
	dev->zbd_info.zbd_opt_nr_open_seq_pref =
		zbc_sg_get_int32(&buf[8]);
	dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref =
		zbc_sg_get_int32(&buf[12]);
	dev->zbd_info.zbd_max_nr_open_seq_req =
		zbc_sg_get_int32(&buf[16]);

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
	fd = open(filename, flags);
	if (fd < 0) {
		ret = -errno;
		zbc_error("Open device file %s failed %d (%s)\n",
			  filename,
			  errno, strerror(errno));
		goto out;
	}

	/* Check device */
	if (fstat(fd, &st) != 0) {
		ret = -errno;
		zbc_error("Stat device %s failed %d (%s)\n",
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
static ssize_t zbc_scsi_pread(struct zbc_device *dev, void *buf,
			      size_t count, uint64_t offset)
{
	size_t sz = count << 9;
	struct zbc_sg_cmd cmd;
	ssize_t ret;

	/* READ 16 */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_READ, buf, sz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

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
static ssize_t zbc_scsi_pwrite(struct zbc_device *dev, const void *buf,
			       size_t count, uint64_t offset)
{
	size_t sz = count << 9;
	struct zbc_sg_cmd cmd;
	ssize_t ret;

	/* WRITE 16 */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_WRITE, (uint8_t *)buf, sz);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

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
static int zbc_scsi_flush(struct zbc_device *dev)
{
	struct zbc_sg_cmd cmd;
	int ret;

	/* SYNCHRONIZE CACHE 16 */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SYNC_CACHE, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

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
 * ZBC with SCSI I/O device operations.
 */
struct zbc_ops zbc_scsi_ops =
{
	.zbd_open		= zbc_scsi_open,
	.zbd_close		= zbc_scsi_close,
	.zbd_pread		= zbc_scsi_pread,
	.zbd_pwrite		= zbc_scsi_pwrite,
	.zbd_flush		= zbc_scsi_flush,
	.zbd_report_zones	= zbc_scsi_report_zones,
	.zbd_zone_op		= zbc_scsi_zone_op,
	.zbd_set_zones		= zbc_scsi_set_zones,
	.zbd_set_wp		= zbc_scsi_set_write_pointer,
};

