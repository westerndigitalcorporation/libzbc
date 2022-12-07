/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
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
