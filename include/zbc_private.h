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
 * Authors: Christoph Hellwig (hch@infradead.org)
 */

#ifndef _LIBZBC_PRIVATE_H_
#define _LIBZBC_PRIVATE_H_

/**
 * zbc_set_zones - Configure zones of a "hacked" ZBC device
 * @dev:                (IN) ZBC device handle of the device to configure
 * @conv_sz:            (IN) Size in logical sectors of the conventional zone (zone 0). This can be 0.
 * @seq_sz:             (IN) Size in logical sectors of sequential write required zones. This cannot be 0.
 *
 * This executes the non-standard SET ZONES command to change the zone configuration of a ZBC drive.
 */
extern int
zbc_set_zones(struct zbc_device *dev,
              uint64_t conv_sz,
              uint64_t seq_sz);

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 * @dev:                (IN) ZBC device handle of the device to configure
 * @zone:               (IN) The zone to configure
 * @wp_lba:             (IN) New value of the write pointer (must be at least equal to the zone start LBA
 *                           (zone empty) and at most equal to the zone last LBA plus 1 (zone full).
 *
 * This executes the non-standard SET ZONES command to change the zone configuration of a ZBC drive.
 */
extern int
zbc_set_write_pointer(struct zbc_device *dev,
                      uint64_t start_lba,
                      uint64_t wp_lba);

#endif /* _LIBZBC_PRIVATE_H_ */
