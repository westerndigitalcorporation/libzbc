#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2023, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "READ across conventional zone boundary" $*

# Get drive information
zbc_test_get_device_info

# Search target LBA
test_zone_type="${ZT_CONV}"
zbc_test_get_zones_cond_or_NA "NOT_WP" "NOT_WP"
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
