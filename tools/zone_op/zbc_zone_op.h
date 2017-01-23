/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public License version 3, "as is," without technical
 * support, and WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
 * received a copy of the GNU Lesser General Public License along with libzbc.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */

#ifndef _ZBC_ZONE_OP_H_
#define _ZBC_ZONE_OP_H_

#include <libzbc/zbc.h>

/**
 * Execute a zone operation.
 */
extern int zbc_zone_op(char *bin_name, enum zbc_zone_op op,
		       int argc, char **argv);

#endif /* _ZBC_ZONE_OP_H_ */
