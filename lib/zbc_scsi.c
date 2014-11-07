/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc.  This software is distributed
 * under the terms of the GNU Lesser General Public License version 3,
 * or any later version, "as is," without technical support, and WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  You should have received a copy
 * of the GNU Lesser General Public License along with libzbc.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: Damien Le Moal (damien.lemoal@hgst.com)
 *          Christophe Louargant (christophe.louargant@hgst.com)
 */

/***** Including files *****/

#include "zbc.h"
#include "zbc_sg.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

/***** Macro definitions *****/

/**
 * Number of bytes in a Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_LENGTH              64

/**
 * Number of bytes in the buffer before the first Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_OFFSET              64

/**
 * ZBC Device types.
 */
#define ZBC_DEV_TYPE_STANDARD                   0x0
#define ZBC_DEV_TYPE_HOST_MANAGED               0x14

/***** Definition of private functions *****/

/**
 * Get information (model, vendor, ...) from a SCSI device.
 */
static int
zbc_scsi_classify(zbc_device_t *dev)
{
    zbc_sg_cmd_t cmd;
    int dev_type;
    int ret;

    /* Allocate and intialize inquiry command */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_INQUIRY, NULL, ZBC_SG_INQUIRY_REPLY_LEN);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
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
    if ( ret != 0 ) {
	goto out;
    }

    /* Make sure we are not dealing with an ATA device */
    if ( strncmp((char *)&cmd.out_buf[8], "ATA", 3) == 0 ) {
	ret = -ENXIO;
	goto out;
    }

    /* This is a SCSI device */
    dev->zbd_info.zbd_type = ZBC_DT_SCSI;

    /* Now check the device type */
    dev_type = (int)(cmd.out_buf[0] & 0x1f);

    if ( dev_type == ZBC_DEV_TYPE_HOST_MANAGED ) {
        /* Host-managed device */
        dev->zbd_info.zbd_model = ZBC_DM_HOST_MANAGED;
	goto out;
    }

    if ( dev_type != ZBC_DEV_TYPE_STANDARD ) {
	/* Unsupported device */
	ret = -ENXIO;
	goto out;
    }

    /* This may be a host-managed device: look at VPD    */
    /* page B5h (block device characteristics extension) */
    memset(cmd.cdb, 0, sizeof(cmd.cdb));
    memset(cmd.out_buf, 0, ZBC_SG_INQUIRY_REPLY_LEN);
    cmd.cdb[0] = ZBC_SG_INQUIRY_CDB_OPCODE;
    zbc_sg_cmd_set_int16(&cmd.cdb[3], ZBC_SG_INQUIRY_REPLY_LEN);
    cmd.cdb[1] = 0x01;
    cmd.cdb[2] = 0xB5;
    zbc_sg_cmd_set_int32(&cmd.cdb[3], ZBC_SG_INQUIRY_REPLY_LEN);

    /* Execute the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);
    if ( ret == 0 ) {

	if ( cmd.out_buf[8] & 0x10 ) {
	    /* Host aware device */
	    dev->zbd_info.zbd_model = ZBC_DM_HOST_AWARE;
	} else {
	    /* Standard or drive-managed device */
	    dev->zbd_info.zbd_model = ZBC_DM_DRIVE_MANAGED;
	}

    }

out:

    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Get a device information (capacity & sector sizes).
 */
static int
zbc_scsi_get_info(zbc_device_t *dev)
{
    zbc_sg_cmd_t cmd;
    int logical_per_physical;
    int ret;

    /* Get device model */
    ret = zbc_scsi_classify(dev);
    if ( ret != 0 ) {
        return( ret );
    }

    if ( dev->zbd_info.zbd_model == ZBC_DM_DRIVE_MANAGED ) {
        /* Non-SMR or drive managed device... Nothing to do with it */
        return( -ENXIO );
    }

    /* READ CAPACITY 16 */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_READ_CAPACITY, NULL, ZBC_SG_READ_CAPACITY_REPLY_LEN);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
    }

    /* Fill command CDB */
    cmd.cdb[0] = ZBC_SG_READ_CAPACITY_CDB_OPCODE;
    cmd.cdb[1] = ZBC_SG_READ_CAPACITY_CDB_SA;
    zbc_sg_cmd_set_int32(&cmd.cdb[10], ZBC_SG_READ_CAPACITY_REPLY_LEN);

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);
    if ( ret != 0 ) {
        goto out;
    }

    dev->zbd_info.zbd_logical_blocks = zbc_sg_cmd_get_int64(&cmd.out_buf[0]) + 1;
    dev->zbd_info.zbd_logical_block_size = zbc_sg_cmd_get_int32(&cmd.out_buf[8]);
    logical_per_physical = 1 << cmd.out_buf[13] & 0x0f;

    /* Check */
    if ( dev->zbd_info.zbd_logical_block_size <= 0 ) {
        zbc_error("%s: invalid logical sector size\n",
                  dev->zbd_filename);
        ret = -EINVAL;
        goto out;
    }

    if ( ! dev->zbd_info.zbd_logical_blocks ) {
        zbc_error("%s: invalid capacity (logical blocks)\n",
                  dev->zbd_filename);
        ret = -EINVAL;
        goto out;
    }

    dev->zbd_info.zbd_physical_block_size = dev->zbd_info.zbd_logical_block_size * logical_per_physical;
    dev->zbd_info.zbd_physical_blocks = dev->zbd_info.zbd_logical_blocks / logical_per_physical;

out:

    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

static int
zbc_scsi_open(const char *filename,
              int flags,
              struct zbc_device **pdev)
{
    struct zbc_device *dev;
    struct stat st;
    int fd, ret;

    /* Open the device file */
    fd = open(filename, flags);
    if ( fd < 0 ) {
        zbc_error("Open device file %s failed %d (%s)\n",
                  filename,
                  errno,
                  strerror(errno));
        return -errno;
    }

    /* Check device */
    if ( fstat(fd, &st) != 0 ) {
        zbc_error("Stat device %s failed %d (%s)\n",
                  filename,
                  errno,
                  strerror(errno));
        ret = -errno;
        goto out;
    }

    if ( (! S_ISCHR(st.st_mode))
         && (! S_ISBLK(st.st_mode)) ) {
        ret = -ENXIO;
        goto out;
    }

    /* Set device decriptor */
    ret = -ENOMEM;
    dev = calloc(1, sizeof(struct zbc_device));
    if ( ! dev ) {
        goto out;
    }

    dev->zbd_filename = strdup(filename);
    if ( ! dev->zbd_filename ) {
        goto out_free_dev;
    }

    dev->zbd_fd = fd;

    ret = zbc_scsi_get_info(dev);
    if ( ret ) {
        goto out_free_filename;
    }

    *pdev = dev;

    return 0;

out_free_filename:

    free(dev->zbd_filename);

out_free_dev:

    free(dev);

out:

    close(fd);

    return ret;

}

static int
zbc_scsi_close(zbc_device_t *dev)
{

    if ( close(dev->zbd_fd) ) {
        return -errno;
    }

    free(dev->zbd_filename);
    free(dev);

    return 0;

}

/**
 * Read from a ZBC device
 */
static int32_t
zbc_scsi_pread(zbc_device_t *dev,
               zbc_zone_t *zone,
               void *buf,
               uint32_t lba_count,
               uint64_t lba_ofst)
{
    size_t sz = (size_t) lba_count * dev->zbd_info.zbd_logical_block_size;
    zbc_sg_cmd_t cmd;
    int ret;

    /* READ 16 */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_READ, buf, sz);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
    }

    /* Fill command CDB */
    cmd.cdb[0] = ZBC_SG_READ_CDB_OPCODE;
    cmd.cdb[1] = 0x10;
    zbc_sg_cmd_set_int64(&cmd.cdb[2], (zone->zbz_start + lba_ofst));
    zbc_sg_cmd_set_int32(&cmd.cdb[10], lba_count);

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);
    if ( ret == 0 ) {
        ret = (sz - cmd.io_hdr.resid) / dev->zbd_info.zbd_logical_block_size;
    }

    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Write to a ZBC device
 */
static int32_t
zbc_scsi_pwrite(zbc_device_t *dev,
                zbc_zone_t *zone,
                const void *buf,
                uint32_t lba_count,
                uint64_t lba_ofst)
{
    size_t sz = (size_t) lba_count * dev->zbd_info.zbd_logical_block_size;
    zbc_sg_cmd_t cmd;
    int ret;

    /* WRITE 16 */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_WRITE, (uint8_t *)buf, sz);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
    }

    /* Fill command CDB */
    cmd.cdb[0] = ZBC_SG_WRITE_CDB_OPCODE;
    cmd.cdb[1] = 0x10;
    zbc_sg_cmd_set_int64(&cmd.cdb[2], (zone->zbz_start + lba_ofst));
    zbc_sg_cmd_set_int32(&cmd.cdb[10], lba_count);

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);
    if ( ret == 0 ) {
        ret = (sz - cmd.io_hdr.resid) / dev->zbd_info.zbd_logical_block_size;
    }

    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Flush a ZBC device cache.
 */
static int
zbc_scsi_flush(zbc_device_t *dev,
               uint64_t lba_ofst,
               uint32_t lba_count,
               int immediate)
{
    zbc_sg_cmd_t cmd;
    int ret;

    /* SYNCHRONIZE CACHE 16 */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SYNC_CACHE, NULL, 0);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
    }

    /* Fill command CDB */
    cmd.cdb[0] = ZBC_SG_SYNC_CACHE_CDB_OPCODE;
    if ( lba_ofst ) {
        zbc_sg_cmd_set_int64(&cmd.cdb[2], lba_ofst);
    }
    if ( lba_count ) {
        zbc_sg_cmd_set_int32(&cmd.cdb[10], lba_count);
    }
    if ( immediate ) {
        cmd.cdb[1] = 0x02;
    }

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);

    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Get device zone information.
 */
static int
zbc_scsi_report_zones(zbc_device_t *dev,
                      uint64_t start_lba,
                      enum zbc_reporting_options ro,
                      zbc_zone_t *zones,
                      unsigned int *nr_zones)
{
    size_t out_bufsz = ZBC_ZONE_DESCRIPTOR_OFFSET;
    zbc_sg_cmd_t cmd;
    uint8_t *buf;
    int i, nz, ret, reported_zones;

    if ( *nr_zones ) {
        zbc_debug("Report at most %d zones\n",
                 *nr_zones);
        out_bufsz += *nr_zones * ZBC_ZONE_DESCRIPTOR_LENGTH;
        if ( out_bufsz > sysconf(_SC_PAGESIZE) ) {
            out_bufsz = sysconf(_SC_PAGESIZE);
            zbc_debug("Limit zone report to %d / %d zones\n",
                     (int)((out_bufsz - ZBC_ZONE_DESCRIPTOR_OFFSET) / ZBC_ZONE_DESCRIPTOR_LENGTH),
                     *nr_zones);
            *nr_zones = (out_bufsz - ZBC_ZONE_DESCRIPTOR_OFFSET) / ZBC_ZONE_DESCRIPTOR_LENGTH;
        }
    }

    /* Allocate and intialize report zones command */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_REPORT_ZONES, NULL, out_bufsz);
    if ( ret != 0 ) {
        zbc_error("zbc_sg_cmd_init failed\n");
        return( ret );
    }

    /* Fill command CDB:
     * +=============================================================================+
     * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
     * |Byte |        |        |        |        |        |        |        |        |
     * |=====+==========================+============================================|
     * | 0   |                           Operation Code (9Eh)                        |
     * |-----+-----------------------------------------------------------------------|
     * | 1   |      Reserved            |       Service Action (14h)                 |
     * |-----+-----------------------------------------------------------------------|
     * | 2   | (MSB)                                                                 |
     * |- - -+---                        Zone Start LBA                           ---|
     * | 9   |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 10  | (MSB)                                                                 |
     * |- - -+---                        Allocation Length                        ---|
     * | 13  |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 14  |               Reserved            |       Reporting Options           |
     * |-----+-----------------------------------------------------------------------|
     * | 15  |                           Control                                     |
     * +=============================================================================+
     */
    cmd.cdb[0] = ZBC_SG_REPORT_ZONES_CDB_OPCODE;
    cmd.cdb[1] = ZBC_SG_REPORT_ZONES_CDB_SA;
    zbc_sg_cmd_set_int64(&cmd.cdb[2], start_lba);
    zbc_sg_cmd_set_int32(&cmd.cdb[10], out_bufsz);
    cmd.cdb[14] = ro & 0x0f;

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);
    if ( ret != 0 ) {
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
     * |  4  |                           Reserved                           | Same   |
     * |-----+-----------------------------------------------------------------------|
     * |  5  | (MSB)                                                                 |
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
    reported_zones = (out_bufsz - ZBC_ZONE_DESCRIPTOR_OFFSET) / ZBC_ZONE_DESCRIPTOR_LENGTH;

    if ( zones && reported_zones ) {

        /* Get zone info */
        if ( nz > *nr_zones ) {
            nz = *nr_zones;
        }
        if ( nz > reported_zones ) {
            nz = reported_zones;
        }

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
        for(i = 0; i < nz; i++) {

            zones[i].zbz_type = buf[0] & 0x0f;
            zones[i].zbz_condition = (buf[1] >> 4) & 0x0f;
            zones[i].zbz_length = zbc_sg_cmd_get_int64(&buf[8]);
            zones[i].zbz_start = zbc_sg_cmd_get_int64(&buf[16]);
            zones[i].zbz_write_pointer = zbc_sg_cmd_get_int64(&buf[24]);
            zones[i].zbz_need_reset = (buf[1] & 0x01) ? true : false;;
            zones[i].zbz_non_seq= (buf[1] & 0x02) ? true : false;

            buf += ZBC_ZONE_DESCRIPTOR_LENGTH;

        }

    }

    /* Return number of zones */
    *nr_zones = nz;

out:

    /* Cleanup */
    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Reset zone(s) write pointer.
 */
static int
zbc_scsi_reset_write_pointer(zbc_device_t *dev,
                             uint64_t start_lba)
{
    zbc_sg_cmd_t cmd;
    int ret;

    /* Allocate and intialize reset write pointer command */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_RESET_WRITE_POINTER, NULL, 0);
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
     * | 1   |      Reserved            |       Service Action (14h)                 |
     * |-----+-----------------------------------------------------------------------|
     * | 2   | (MSB)                                                                 |
     * |- - -+---                        Zone ID                                  ---|
     * | 9   |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 10  | (MSB)                                                                 |
     * |- - -+---                        Reserved                                 ---|
     * | 13  |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 14  |               Reserved                                       | Reset  |
     * |-----+-----------------------------------------------------------------------|
     * | 15  |                           Control                                     |
     * +=============================================================================+
     */
    cmd.cdb[0] = ZBC_SG_RESET_WRITE_POINTER_CDB_OPCODE;
    cmd.cdb[1] = ZBC_SG_RESET_WRITE_POINTER_CDB_SA;
    if ( start_lba == (uint64_t)-1 ) {
        /* Reset ALL zones */
        cmd.cdb[14] = 0x01;
    } else {
        /* Reset only the zone at start_lba */
        zbc_sg_cmd_set_int64(&cmd.cdb[2], start_lba);
    }

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);

    /* Cleanup */
    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Configure zones of a "emulated" ZBC device
 */
static int
zbc_scsi_set_zones(zbc_device_t *dev,
                   uint64_t conv_sz,
                   uint64_t seq_sz)
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
     * |- - -+---             Sequential Zone Sise (LBA)                          ---|
     * | 15  |                                                                 (LSB) |
     * +=============================================================================+
     */
    cmd.cdb[0] = ZBC_SG_SET_ZONES_CDB_OPCODE;
    cmd.cdb[1] = ZBC_SG_SET_ZONES_CDB_SA;
    zbc_sg_cmd_set_bytes(&cmd.cdb[2], &conv_sz, 7);
    zbc_sg_cmd_set_bytes(&cmd.cdb[9], &seq_sz, 7);

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);

    /* Cleanup */
    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * Change the value of a zone write pointer ("emulated" ZBC devices only).
 */
static int
zbc_scsi_set_write_pointer(zbc_device_t *dev,
                           uint64_t start_lba,
                           uint64_t write_pointer)
{
    zbc_sg_cmd_t cmd;
    int ret;

    /* Allocate and intialize set zone command */
    ret = zbc_sg_cmd_init(&cmd, ZBC_SG_SET_WRITE_POINTER, NULL, 0);
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
    zbc_sg_cmd_set_bytes(&cmd.cdb[9], &write_pointer, 7);

    /* Send the SG_IO command */
    ret = zbc_sg_cmd_exec(dev, &cmd);

    /* Cleanup */
    zbc_sg_cmd_destroy(&cmd);

    return( ret );

}

/**
 * ZBC with SCSI I/O device operations.
 */
zbc_ops_t zbc_scsi_ops =
{
    .zbd_open         = zbc_scsi_open,
    .zbd_close        = zbc_scsi_close,
    .zbd_pread        = zbc_scsi_pread,
    .zbd_pwrite       = zbc_scsi_pwrite,
    .zbd_flush        = zbc_scsi_flush,
    .zbd_report_zones = zbc_scsi_report_zones,
    .zbd_reset_wp     = zbc_scsi_reset_write_pointer,
    .zbd_set_zones    = zbc_scsi_set_zones,
    .zbd_set_wp       = zbc_scsi_set_write_pointer,
};

