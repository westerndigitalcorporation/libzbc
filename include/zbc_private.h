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
 * Authors: Christoph Hellwig (hch@infradead.org)
 *          Damien Le Moal (damien.lemoal@wdc.com)
 */

#ifndef _LIBZBC_PRIVATE_H_
#define _LIBZBC_PRIVATE_H_

/**
 * zbc_set_zones - Configure zones of an emulated ZBC device
 * @dev:	(IN) Device handle of the device to configure
 * @conv_sz:	(IN) Total size in 512B sectors of conventional zones
 * @zone_sz:	(IN) Size in 512B sectors of zones.
 *
 * Description:
 * This function only affects devices operating with the emulation (fake)
 * backend driver. zbc_set_zones allows initializing or changing the zone
 * configuration of the emulated host-managed device. @conv_sz specifies
 * the total size of the device conventional space. This can be 0. @zone_sz
 * specifies the size of the device zones. @conv_sz / @zone_sz gives the
 * number of conventional zones that the emulated device will have. The
 * total number of sequential write required zones will be determined using
 * the remaining capacity of the backend device or file used for emulation.
 */
extern int zbc_set_zones(struct zbc_device *dev,
			 uint64_t conv_sz, uint64_t zone_sz);

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 * @dev:	(IN) ZBC device handle of the device to configure
 * @sector:	(IN) The starting 512B sector of the zone to configure
 * @wp_sector:	(IN) New 512B sector value of the zone write pointer
 *
 * Description:
 * This function only affects devices operating with the emulation (fake)
 * backend driver and allows changing the write pointer position of a
 * sequential write required zone of the emulated device. If @wp_sector
 * is not within the sector range of the zone identified by @sector,
 * the zone write pointer is set to -1 (invalid) and the zone condition
 * is set to FULL.
 */
extern int zbc_set_write_pointer(struct zbc_device *dev,
				 uint64_t sector, uint64_t wp_sector);

/**
 * zbc_set_test_mode - Set library calls to test mode for the device
 * @dev:	(IN) Device handle obtained with zbc_open
 *
 * Description:
 * Set the specified device in test mode to reduce libaray internal API
 * arguments checks so that invalid commands can also be sent to the device.
 * For testing the device with zbc_test.sh only.
 */
extern void zbc_set_test_mode(struct zbc_device *dev);

#endif /* _LIBZBC_PRIVATE_H_ */
