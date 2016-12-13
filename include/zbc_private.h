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
 * zbc_set_zones - Configure zones of an emulated ZBC device
 * @dev:      (IN) Device handle of the device to configure
 * @conv_sz:  (IN) Size in logical sectors of the space occupied by
 *                 conventional zones starting at LBA 0. This can be 0.
 * @zone_sz:  (IN) Size in logical sectors of conventional and sequential
 *                 write required zones. This cannot be 0.
 *
 * Description:
 * This function only affects devices operating with the emulation (fake)
 * backend driver.
 */
extern int zbc_set_zones(struct zbc_device *dev,
			 uint64_t conv_sz, uint64_t zone_sz);

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 * @dev:        (IN) ZBC device handle of the device to configure
 * @start_lba:  (IN) The starting LBA of the zone to configure
 * @wp_lba:     (IN) New value of the zone write pointer. If the LBA is not
 *                   within the zone LBA range, the zone write pointer LBA
 *                   is set to -1 and the zone condition to FULL.
 *
 * Description:
 * This function only affects devices operating with the emulation (fake)
 * backend driver.
 */
extern int zbc_set_write_pointer(struct zbc_device *dev,
				 uint64_t start_lba, uint64_t wp_lba);

#endif /* _LIBZBC_PRIVATE_H_ */
