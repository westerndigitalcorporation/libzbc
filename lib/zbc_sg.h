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

#include <scsi/scsi.h>
#include <scsi/sg.h>

/***** Macro definitions *****/

/**
 * SG SCSI command names.
 */
enum {

    ZBC_SG_INQUIRY = 0,
    ZBC_SG_READ_CAPACITY,
    ZBC_SG_READ,
    ZBC_SG_WRITE,
    ZBC_SG_SYNC_CACHE,
    ZBC_SG_REPORT_ZONES,
    ZBC_SG_RESET_WRITE_POINTER,
    ZBC_SG_SET_ZONES,
    ZBC_SG_SET_WRITE_POINTER,
    ZBC_SG_ATA12,
    ZBC_SG_ATA16,

    ZBC_SG_CMD_NUM,

};

/**
 * Inquiry command definition.
 */
#define ZBC_SG_INQUIRY_CDB_OPCODE               0x12
#define ZBC_SG_INQUIRY_CDB_SA                   0x12
#define ZBC_SG_INQUIRY_CDB_LENGTH               6
#define ZBC_SG_INQUIRY_REPLY_LEN                96

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
#define ZBC_SG_REPORT_ZONES_CDB_OPCODE          0x9E
#define ZBC_SG_REPORT_ZONES_CDB_SA              0x14
#define ZBC_SG_REPORT_ZONES_CDB_LENGTH          16

/**
 * Reset write pointer command definition.
 */
#define ZBC_SG_RESET_WRITE_POINTER_CDB_OPCODE   0x9F
#define ZBC_SG_RESET_WRITE_POINTER_CDB_SA       0x14
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
#define ZBC_SG_DRIVER_SENSE         		0x08

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

#endif /* __LIBZBC_SG_H__ */
