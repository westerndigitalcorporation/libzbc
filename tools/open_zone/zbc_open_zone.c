// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital COrporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>

#include <libzbc/zbc.h>
#include "../zone_op/zbc_zone_op.h"

int main(int argc, char **argv)
{
	return zbc_zone_op(argv[0], ZBC_OP_OPEN_ZONE,
			   argc - 1, &argv[1]);
}

