// SPDX-License-Identifier: BSD-2-Clause
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * Authors: Christoph Hellwig (hch@infradead.org)
 *          Damien Le Moal (damien.lemoal@wdc.com)
 */

#ifndef _LIBZBC_PRIVATE_H_
#define _LIBZBC_PRIVATE_H_

/**
 * @brief User hidden ZBC device open flags
 */
enum zbc_oflags_internal {

	/** Open device in test mode */
	ZBC_O_SETZONES		= 0x20000000,
	ZBC_O_DEVTEST		= 0x40000000,
	ZBC_O_DIRECT		= 0x80000000,

};

#endif /* _LIBZBC_PRIVATE_H_ */
