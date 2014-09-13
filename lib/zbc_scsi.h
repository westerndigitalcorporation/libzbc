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

#ifndef __LIBZBC_SCSI_H__
#define __LIBZBC_SCSI_H__

/***** Including files *****/

#include "zbc.h"

#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

/***** Macro definitions *****/

/**
 * Number of bytes in a Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_LENGTH              64

/**
 * Number of bytes in the buffer before the first Zone Descriptor.
 */
#define ZBC_ZONE_DESCRIPTOR_OFFSET              64

/***** Internal command functions *****/

/**
 * INQUIRY a SCSI device.
 */
extern int
zbc_scsi_inquiry(zbc_device_t *dev,
                 uint8_t **pbuf,
                 int *dev_model);

#endif /* __LIBZBC_SCSI_H__ */
