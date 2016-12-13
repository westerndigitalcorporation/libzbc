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
 *          Christophe Louargant (christophe.louargant@hgst.com)
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
 * ZBC Device types.
 */
#define ZBC_DEV_TYPE_STANDARD		0x00
#define ZBC_DEV_TYPE_HOST_MANAGED	0x14

/**
 * Get information (model, vendor, ...) from a SCSI device.
 */
static int zbc_scsi_classify(zbc_device_t *dev)
{
	zbc_sg_cmd_t cmd;
	int dev_type;
	int n, ret;

	/* Allocate and intialize inquiry command */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_INQUIRY, NULL, ZBC_SG_INQUIRY_REPLY_LEN);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
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
	if (ret != 0)
		goto out;

	/* Make sure we are not dealing with an ATA device */
	if (strncmp((char *)&cmd.out_buf[8], "ATA", 3) == 0) {
		ret = -ENXIO;
		goto out;
	}

	/* This is a SCSI device */
	dev->zbd_info.zbd_type = ZBC_DT_SCSI;

	/* Vendor identification */
	n = zbc_sg_cmd_strcpy(&dev->zbd_info.zbd_vendor_id[0],
			      (char *)&cmd.out_buf[8], 8);

	/* Product identification */
	n += zbc_sg_cmd_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			       (char *)&cmd.out_buf[16], 16);

	/* Product revision */
	n += zbc_sg_cmd_strcpy(&dev->zbd_info.zbd_vendor_id[n],
			       (char *)&cmd.out_buf[32], 4);

	/* Now check the device type */
	dev_type = (int)(cmd.out_buf[0] & 0x1f);
	switch (dev_type) {

	case ZBC_DEV_TYPE_HOST_MANAGED:
		/* Host-managed device */
		zbc_debug("Host-managed ZBC disk signature detected\n");
		dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
		goto out;

	case ZBC_DEV_TYPE_STANDARD:
		zbc_debug("Standard SCSI disk signature detected\n");
		break;

	default:
		/* Unsupported device */
		ret = -ENXIO;
		goto out;

	}

	/*
	 * This may be a host-aware device: look at VPD
	 * page B1h (block device characteristics)
	 */
	memset(cmd.cdb, 0, sizeof(cmd.cdb));
	memset(cmd.out_buf, 0, ZBC_SG_INQUIRY_REPLY_LEN_VPD_PAGE_B1);
	cmd.cdb[0] = ZBC_SG_INQUIRY_CDB_OPCODE;
	cmd.cdb[1] = 0x01;
	cmd.cdb[2] = 0xB1;
	zbc_sg_cmd_set_int16(&cmd.cdb[3], ZBC_SG_INQUIRY_REPLY_LEN_VPD_PAGE_B1);

	/* Execute the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	if ((cmd.out_buf[1] == 0xB1) &&
	    (cmd.out_buf[2] == 0x00) &&
	    (cmd.out_buf[3] == 0x3C)) {

		switch ((cmd.out_buf[8] & 0x30) >> 4) {

		case 0x00:
			zbc_debug("Standard SCSI block device detected\n");
			dev->zbd_info.zbd_model = ZBC_DM_STANDARD;
			ret = -ENXIO;
			break;
		case 0x01:
			/* Host aware device */
			zbc_debug("Host-aware ZBC block device detected\n");
			dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
			break;

		case 0x10:
			/* Device-managed device */
			zbc_debug("Device managed SCSI block device detected\n");
			dev->zbd_info.zbd_model = ZBC_DM_DEVICE_MANAGED;
			ret = -ENXIO;
			break;

		default:
			zbc_debug("Unknown device type\n");
			dev->zbd_info.zbd_model = ZBC_DM_DRIVE_UNKNOWN;
			ret = -ENXIO;
			break;

		}

	}

out:
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get a SCSI device zone information.
 */
static int zbc_scsi_do_report_zones(zbc_device_t *dev, uint64_t start_lba,
				    enum zbc_reporting_options ro,
				    uint64_t *max_lba,
				    zbc_zone_t *zones, unsigned int *nr_zones)
{
	size_t bufsz = ZBC_ZONE_DESCRIPTOR_OFFSET;
	unsigned int i, nz = 0, buf_nz;
	zbc_sg_cmd_t cmd;
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
		return( ret );
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
	zbc_sg_cmd_set_int64(&cmd.cdb[2], start_lba);
	zbc_sg_cmd_set_int32(&cmd.cdb[10], (unsigned int) bufsz);
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
	nz = zbc_sg_cmd_get_int32(buf) / ZBC_ZONE_DESCRIPTOR_LENGTH;
	if (max_lba)
		*max_lba = zbc_sg_cmd_get_int64(&buf[8]);

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
		zones[i].zbz_condition = (buf[1] >> 4) & 0x0f;
		zones[i].zbz_length = zbc_sg_cmd_get_int64(&buf[8]);
		zones[i].zbz_start = zbc_sg_cmd_get_int64(&buf[16]);
		zones[i].zbz_write_pointer =
			zbc_sg_cmd_get_int64(&buf[24]);
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
 * Get a SCSI device zone information.
 */
static int zbc_scsi_report_zones(zbc_device_t *dev, uint64_t start_lba,
				 enum zbc_reporting_options ro,
				 zbc_zone_t *zones, unsigned int *nr_zones)
{
	return zbc_scsi_do_report_zones(dev, start_lba, ro,
					NULL, zones, nr_zones);
}

/**
 * Zone(s) operation.
 */
int zbc_scsi_zone_op(zbc_device_t *dev, uint64_t start_lba,
		     enum zbc_zone_op op, unsigned int flags)
{
	unsigned int opid;
	unsigned int opcode;
	unsigned int sa;
	zbc_sg_cmd_t cmd;
	int ret;

	switch (op) {
	case ZBC_OP_OPEN_ZONE:
		opid = ZBC_SG_OPEN_ZONE;
		opcode = ZBC_SG_OPEN_ZONE_CDB_OPCODE;
		sa = ZBC_SG_OPEN_ZONE_CDB_SA;
		break;
	case ZBC_OP_CLOSE_ZONE:
		opid = ZBC_SG_CLOSE_ZONE;
		opcode = ZBC_SG_CLOSE_ZONE_CDB_OPCODE;
		sa = ZBC_SG_CLOSE_ZONE_CDB_SA;
		break;
	case ZBC_OP_FINISH_ZONE:
		opid = ZBC_SG_FINISH_ZONE;
		opcode = ZBC_SG_FINISH_ZONE_CDB_OPCODE;
		sa = ZBC_SG_FINISH_ZONE_CDB_SA;
		break;
	case ZBC_OP_RESET_ZONE:
		opid = ZBC_SG_CLOSE_ZONE;
		opcode = ZBC_SG_CLOSE_ZONE_CDB_OPCODE;
		sa = ZBC_SG_CLOSE_ZONE_CDB_SA;
		break;
	default:
		return -EINVAL;
	}

	/* Allocate and intialize zone command */
	ret = zbc_sg_cmd_init(&cmd, opid, NULL, 0);
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
	cmd.cdb[0] = opcode;
	cmd.cdb[1] = sa;
	if (flags & ZBC_OP_ALL_ZONES)
		/* Operate on all zones */
		cmd.cdb[14] = 0x01;
	else
		/* Operate on the zone at start_lba */
		zbc_sg_cmd_set_int64(&cmd.cdb[2], start_lba);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Configure zones of a "emulated" ZBC device
 */
static int zbc_scsi_set_zones(zbc_device_t *dev,
			      uint64_t conv_sz, uint64_t zone_sz)
{
    zbc_sg_cmd_t cmd;
    int ret;

    /* Allocate and intialize set zone command */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SET_ZONES, NULL, 0);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
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
    zbc_sg_cmd_set_bytes(&cmd.cdb[2], &conv_sz, 7);
    zbc_sg_cmd_set_bytes(&cmd.cdb[9], &zone_sz, 7);

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);

    /* Cleanup */
    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Change the value of a zone write pointer ("emulated" ZBC devices only).
 */
static int zbc_scsi_set_write_pointer(zbc_device_t *dev,
				      uint64_t start_lba, uint64_t wp_lba)
{
	zbc_sg_cmd_t cmd;
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
	zbc_sg_cmd_set_bytes(&cmd.cdb[2], &start_lba, 7);
	zbc_sg_cmd_set_bytes(&cmd.cdb[9], &wp_lba, 7);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	/* Cleanup */
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get zoned block device characteristics
 * (Maximum or optimum number of open zones).
 */
int zbc_scsi_get_zbd_chars(zbc_device_t *dev)
{
	zbc_sg_cmd_t cmd;
	int ret;

	/* READ CAPACITY 16 */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_INQUIRY, NULL,
			      ZBC_SG_INQUIRY_REPLY_LEN_VPD_PAGE_B6);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB */
	cmd.cdb[0] = ZBC_SG_INQUIRY_CDB_OPCODE;
	cmd.cdb[1] = 0x01;
	cmd.cdb[2] = 0xB6;
	zbc_sg_cmd_set_int16(&cmd.cdb[3], ZBC_SG_INQUIRY_REPLY_LEN_VPD_PAGE_B6);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret != 0)
		goto out;

	/* URSWRZ (unrestricted read in sequential write required zone) flag */
	dev->zbd_info.zbd_flags |= (cmd.out_buf[4] & 0x01) ?
		ZBC_UNRESTRICTED_READ : 0;

	/* Resource of handling zones */
	dev->zbd_info.zbd_opt_nr_open_seq_pref =
		zbc_sg_cmd_get_int32(&cmd.out_buf[8]);
	dev->zbd_info.zbd_opt_nr_non_seq_write_seq_pref =
		zbc_sg_cmd_get_int32(&cmd.out_buf[12]);
	dev->zbd_info.zbd_max_nr_open_seq_req =
		zbc_sg_cmd_get_int32(&cmd.out_buf[16]);

	if ( (dev->zbd_info.zbd_model == ZBC_DM_HOST_MANAGED) &&
	     (dev->zbd_info.zbd_max_nr_open_seq_req <= 0) ) {
		zbc_error("%s: invalid maximum number of open sequential "
			  "write required zones for host-managed device\n",
			  dev->zbd_filename);
		ret = -EINVAL;
	}

out:
	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Get a device information (capacity & sector sizes).
 */
static int zbc_scsi_get_info(zbc_device_t *dev)
{
	int ret;

	/* Make sure the device is ready */
	ret = zbc_sg_cmd_test_unit_ready(dev);
	if (ret != 0)
		return ret;

	/* Get device model */
	ret = zbc_scsi_classify(dev);
	if (ret != 0)
		return ret;

	/* Get capacity information */
	ret = zbc_sg_get_capacity(dev, zbc_scsi_do_report_zones);
	if (ret != 0)
		return ret;

	/* Get zoned block device characteristics */
	ret = zbc_scsi_get_zbd_chars(dev);
	if (ret != 0)
		return ret;

	return 0;
}

/**
 * Open a disk.
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
	dev->zbd_filename = strdup(filename);
	if (!dev->zbd_filename)
		goto out_free_dev;

	ret = zbc_scsi_get_info(dev);
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
 * Close a disk.
 */
static int zbc_scsi_close(zbc_device_t *dev)
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
ssize_t zbc_scsi_pread(zbc_device_t *dev, void *buf,
		       size_t lba_count, uint64_t lba_offset)
{
	size_t sz = lba_count * dev->zbd_info.zbd_lblock_size;
	zbc_sg_cmd_t cmd;
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
	zbc_sg_cmd_set_int64(&cmd.cdb[2], lba_offset);
	zbc_sg_cmd_set_int32(&cmd.cdb[10], lba_count);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret == 0)
		ret = (sz - cmd.io_hdr.resid) / dev->zbd_info.zbd_lblock_size;

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Write to a ZBC device
 */
ssize_t zbc_scsi_pwrite(zbc_device_t *dev, const void *buf,
			size_t lba_count, uint64_t lba_offset)
{
	size_t sz = lba_count * dev->zbd_info.zbd_lblock_size;
	zbc_sg_cmd_t cmd;
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
	zbc_sg_cmd_set_int64(&cmd.cdb[2], lba_offset);
	zbc_sg_cmd_set_int32(&cmd.cdb[10], lba_count);

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);
	if (ret == 0)
		ret = (sz - cmd.io_hdr.resid) / dev->zbd_info.zbd_lblock_size;

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * Flush a ZBC device cache.
 */
static int zbc_scsi_flush(zbc_device_t *dev, uint64_t lba_offset,
			  size_t lba_count, int immediate)
{
	zbc_sg_cmd_t cmd;
	int ret;

	/* SYNCHRONIZE CACHE 16 */
	ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SYNC_CACHE, NULL, 0);
	if (ret != 0) {
		zbc_error("zbc_sg_cmd_init failed\n");
		return ret;
	}

	/* Fill command CDB */
	cmd.cdb[0] = ZBC_SG_SYNC_CACHE_CDB_OPCODE;
	zbc_sg_cmd_set_int64(&cmd.cdb[2], lba_offset);
	zbc_sg_cmd_set_int32(&cmd.cdb[10], lba_count);
	if (immediate)
		cmd.cdb[1] = 0x02;

	/* Send the SG_IO command */
	ret = zbc_sg_cmd_exec(dev, &cmd);

	zbc_sg_cmd_destroy(&cmd);

	return ret;
}

/**
 * ZBC with SCSI I/O device operations.
 */
zbc_ops_t zbc_scsi_ops =
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

