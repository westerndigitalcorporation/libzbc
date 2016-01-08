/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc.  All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Author: Damien Le Moal (damien.lemoal@hgst.com)
 *         Christophe Louargant (christophe.louargant@hgst.com)
 */

#ifndef __LIBZBC_SG_H__
#define __LIBZBC_SG_H__

/***** Including files *****/

#include "zbc.h"

#include <string.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

/***** Macro definitions *****/

/**
 * SG SCSI command names.
 */
enum {

    ZBC_SG_TEST_UNIT_READY = 0,
    ZBC_SG_INQUIRY,
    ZBC_SG_READ_CAPACITY,
    ZBC_SG_READ,
    ZBC_SG_WRITE,
    ZBC_SG_SYNC_CACHE,
    ZBC_SG_REPORT_ZONES,
    ZBC_SG_OPEN_ZONE,
    ZBC_SG_CLOSE_ZONE,
    ZBC_SG_FINISH_ZONE,
    ZBC_SG_RESET_WRITE_POINTER,
    ZBC_SG_SET_ZONES,
    ZBC_SG_SET_WRITE_POINTER,
    ZBC_SG_ATA12,
    ZBC_SG_ATA16,

    ZBC_SG_CMD_NUM,

};

/**
 * Test unit ready command definition.
 */
#define ZBC_SG_TEST_UNIT_READY_CDB_OPCODE       0x00
#define ZBC_SG_TEST_UNIT_READY_CDB_LENGTH       6

/**
 * Inquiry command definition.
 */
#define ZBC_SG_INQUIRY_CDB_OPCODE               0x12
#define ZBC_SG_INQUIRY_CDB_LENGTH               6
#define ZBC_SG_INQUIRY_REPLY_LEN                96
#define ZBC_SG_INQUIRY_REPLY_LEN_VPD_PAGE_B1    64
#define ZBC_SG_INQUIRY_REPLY_LEN_VPD_PAGE_B6    64

/**
 * Read capacity command definition.
 */
#define ZBC_SG_READ_CAPACITY_CDB_OPCODE         0x9E
#define ZBC_SG_READ_CAPACITY_CDB_SA             0x10
#define ZBC_SG_READ_CAPACITY_CDB_LENGTH         16
#define ZBC_SG_READ_CAPACITY_REPLY_LEN          32

/**
 * Read command definition.
 */
#define ZBC_SG_READ_CDB_OPCODE                  0x88
#define ZBC_SG_READ_CDB_LENGTH                  16

/**
 * Write command definition.
 */
#define ZBC_SG_WRITE_CDB_OPCODE                 0x8A
#define ZBC_SG_WRITE_CDB_LENGTH                 16

/**
 * Sync cache command definition.
 */
#define ZBC_SG_SYNC_CACHE_CDB_OPCODE            0x91
#define ZBC_SG_SYNC_CACHE_CDB_LENGTH            16

/**
 * Report zones command definition.
 */
#define ZBC_SG_REPORT_ZONES_CDB_OPCODE          0x95
#define ZBC_SG_REPORT_ZONES_CDB_SA              0x00
#define ZBC_SG_REPORT_ZONES_CDB_LENGTH          16

/**
 * Open zone command definition.
 */
#define ZBC_SG_OPEN_ZONE_CDB_OPCODE             0x94
#define ZBC_SG_OPEN_ZONE_CDB_SA                 0x03
#define ZBC_SG_OPEN_ZONE_CDB_LENGTH             16

/**
 * Close zone command definition.
 */
#define ZBC_SG_CLOSE_ZONE_CDB_OPCODE            0x94
#define ZBC_SG_CLOSE_ZONE_CDB_SA                0x01
#define ZBC_SG_CLOSE_ZONE_CDB_LENGTH            16

/**
 * Finish zone command definition.
 */
#define ZBC_SG_FINISH_ZONE_CDB_OPCODE           0x94
#define ZBC_SG_FINISH_ZONE_CDB_SA               0x02
#define ZBC_SG_FINISH_ZONE_CDB_LENGTH           16

/**
 * Reset write pointer command definition.
 */
#define ZBC_SG_RESET_WRITE_POINTER_CDB_OPCODE   0x94
#define ZBC_SG_RESET_WRITE_POINTER_CDB_SA       0x04
#define ZBC_SG_RESET_WRITE_POINTER_CDB_LENGTH   16

/**
 * Set zones command definition.
 */
#define ZBC_SG_SET_ZONES_CDB_OPCODE             0x9F
#define ZBC_SG_SET_ZONES_CDB_SA                 0x15
#define ZBC_SG_SET_ZONES_CDB_LENGTH             16

/**
 * Set write pointer command definition.
 */
#define ZBC_SG_SET_WRITE_POINTER_CDB_OPCODE     0x9F
#define ZBC_SG_SET_WRITE_POINTER_CDB_SA         0x16
#define ZBC_SG_SET_WRITE_POINTER_CDB_LENGTH     16

/**
 * ATA pass through 12.
 */
#define ZBC_SG_ATA12_CDB_OPCODE			0xA1
#define ZBC_SG_ATA12_CDB_LENGTH			12

/**
 * ATA pass through 16.
 */
#define ZBC_SG_ATA16_CDB_OPCODE			0x85
#define ZBC_SG_ATA16_CDB_LENGTH			16

/**
 * Command sense buffer maximum length.
 */
#define ZBC_SG_SENSE_MAX_LENGTH                 64

/**
 * Maximum command CDB length.
 */
#define ZBC_SG_CDB_MAX_LENGTH                   16

/**
 * Status codes.
 */
#define ZBC_SG_CHECK_CONDITION      		0x02

/**
 * Host status codes.
 */
#define ZBC_SG_DID_OK 				0x00 /* No error */
#define ZBC_SG_DID_NO_CONNECT 			0x01 /* Couldn't connect before timeout period */
#define ZBC_SG_DID_BUS_BUSY 			0x02 /* BUS stayed busy through time out period */
#define ZBC_SG_DID_TIME_OUT 			0x03 /* Timed out for other reason */
#define ZBC_SG_DID_BAD_TARGET 			0x04 /* Bad target, device not responding? */
#define ZBC_SG_DID_ABORT 			0x05 /* Told to abort for some other reason. */
#define ZBC_SG_DID_PARITY 			0x06 /* Parity error. */
#define ZBC_SG_DID_ERROR 			0x07 /* Internal error detected in the host adapter. */
#define ZBC_SG_DID_RESET 			0x08 /* The SCSI bus (or this device) has been reset. */
#define ZBC_SG_DID_BAD_INTR 			0x09 /* Got an unexpected interrupt */
#define ZBC_SG_DID_PASSTHROUGH 			0x0a /* Forced command past mid-layer. */
#define ZBC_SG_DID_SOFT_ERROR 			0x0b /* The low level driver wants a retry. */

/**
 * Driver status codes.
 */
#define ZBC_SG_DRIVER_OK 			0x00
#define ZBC_SG_DRIVER_BUSY 			0x01
#define ZBC_SG_DRIVER_SOFT 			0x02
#define ZBC_SG_DRIVER_MEDIA 			0x03
#define ZBC_SG_DRIVER_ERROR 			0x04
#define ZBC_SG_DRIVER_INVALID 			0x05
#define ZBC_SG_DRIVER_TIMEOUT 			0x06
#define ZBC_SG_DRIVER_HARD 			0x07
#define ZBC_SG_DRIVER_SENSE         		0x08
#define ZBC_SG_DRIVER_STATUS_MASK   		0x0f

/**
 * Driver status code flags ('or'ed with code)
 */
#define ZBC_SG_DRIVER_SUGGEST_RETRY 		0x10
#define ZBC_SG_DRIVER_SUGGEST_ABORT 		0x20
#define ZBC_SG_DRIVER_SUGGEST_REMAP 		0x30
#define ZBC_SG_DRIVER_SUGGEST_DIE 		0x40
#define ZBC_SG_DRIVER_SUGGEST_SENSE 		0x80
#define ZBC_SG_DRIVER_FLAGS_MASK   		0xf0

/***** Type definitions *****/

/**
 * SG command descriptor. Used to process SCSI commands.
*/
typedef struct zbc_sg_cmd {

    int                 code;

    int                 cdb_opcode;
    int                 cdb_sa;
    size_t              cdb_sz;
    uint8_t             cdb[ZBC_SG_CDB_MAX_LENGTH];

    size_t              sense_bufsz;
    uint8_t             sense_buf[ZBC_SG_SENSE_MAX_LENGTH];

    int                 out_buf_needfree;
    size_t              out_bufsz;
    uint8_t             *out_buf;

    sg_io_hdr_t         io_hdr;

} zbc_sg_cmd_t;

#define zbc_sg_cmd_driver_status(cmd)		((cmd)->io_hdr.driver_status & ZBC_SG_DRIVER_STATUS_MASK)
#define zbc_sg_cmd_driver_flags(cmd)		((cmd)->io_hdr.driver_status & ZBC_SG_DRIVER_FLAGS_MASK)

/***** Internal command functions *****/

/**
 * Allocate and initialize a new command.
 */
extern int
zbc_sg_cmd_init(zbc_sg_cmd_t *cmd,
                int cmd_code,
                uint8_t *out_buf,
                size_t out_bufsz);

/**
 * Free a command.
 */
extern void
zbc_sg_cmd_destroy(zbc_sg_cmd_t *cmd);

/**
 * Execute a command.
 */
extern int
zbc_sg_cmd_exec(zbc_device_t *dev,
                zbc_sg_cmd_t *cmd);

/**
 * Test if unit is ready. This will retry 5 times if the command
 * returns "UNIT ATTENTION".
 */
extern int
zbc_sg_cmd_test_unit_ready(zbc_device_t *dev);

/**
 * Fill the buffer with the result of INQUIRY command.
 * buf must be at least ZBC_SG_INQUIRY_REPLY_LEN bytes long.
 */
extern int
zbc_sg_cmd_inquiry(zbc_device_t *dev,
                   void *buf);

/**
 * Get information string from inquiry output.
 */
static inline int
zbc_sg_cmd_strcpy(char *dst,
                  char *buf,
                  int buf_len)
{
    int len = buf_len - 1;

    while( len && ((buf[len] == 0) || (buf[len] == ' ')) ) {
        len--;
    }

    len++;
    strncpy(dst, buf, len);
    dst[len] = ' ';
    dst[len + 1] = '\0';

    return( len + 1);

}

/**
 * Set bytes in a command cdb.
 */
extern void
zbc_sg_cmd_set_bytes(uint8_t *cmd,
                     void *buf,
                     int bytes);

/**
 * Set a 64 bits integer in a command cdb.
 */
static inline void
zbc_sg_cmd_set_int64(uint8_t *buf,
                     uint64_t val)
{

    zbc_sg_cmd_set_bytes(buf, &val, 8);

    return;

}

/**
 * Set a 32 bits integer in a command cdb.
 */
static inline void
zbc_sg_cmd_set_int32(uint8_t *buf,
                     uint32_t val)
{

    zbc_sg_cmd_set_bytes(buf, &val, 4);

    return;

}

/**
 * Set a 16 bits integer in a command cdb.
 */
static inline void
zbc_sg_cmd_set_int16(uint8_t *buf,
                     uint16_t val)
{

    zbc_sg_cmd_set_bytes(buf, &val, 2);

    return;

}

/**
 * Converter structure.
 */
union converter {
    uint8_t         val_buf[8];
    uint16_t        val16;
    uint32_t        val32;
    uint64_t        val64;
};

/**
 * Get bytes from a command output buffer.
 */
extern void
zbc_sg_cmd_get_bytes(uint8_t *val,
                     union converter *conv,
                     int bytes);

/**
 * Get a 64 bits integer from a command output buffer.
 */
static inline uint64_t
zbc_sg_cmd_get_int64(uint8_t *buf)
{
    union converter conv;

    zbc_sg_cmd_get_bytes(buf, &conv, 8);

    return( conv.val64 );

}

/**
 * Get a 32 bits integer from a command output buffer.
 */
static inline uint32_t
zbc_sg_cmd_get_int32(uint8_t *buf)
{
    union converter conv;

    zbc_sg_cmd_get_bytes(buf, &conv, 4);

    return( conv.val32 );

}

/**
 * Get a 16 bits integer from a command output buffer.
 */
static inline uint16_t
zbc_sg_cmd_get_int16(uint8_t *buf)
{
    union converter conv;

    zbc_sg_cmd_get_bytes(buf, &conv, 2);

    return( conv.val16 );

}

/**
 * Print an array of bytes.
 */
extern void
zbc_sg_print_bytes(zbc_device_t *dev,
		   uint8_t *buf,
		   unsigned int len);

#endif /* __LIBZBC_SG_H__ */
