#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE across conventional zone boundary" $*

# Get drive information
zbc_test_get_device_info

# Get a pair of conventional zones we can try to write across their boundary
test_zone_type="${ZT_CONV}"
zbc_test_get_zones_cond_or_NA "NOT_WP" "NOT_WP"
initial_wp=${target_ptr}
expected_cond="${target_cond}"
target_lba=$(( ${target_slba} + ${target_size} - ${lblk_per_pblk} ))

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} \
			$(( ${target_slba} + ${target_size} ))

# Start testing
# Attempt to write through the remaining LBA of the zone and cross over into the next zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			${target_lba} $(( ${lblk_per_pblk} * 2 ))

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}

zbc_test_check_zone_cond "zone_type=${target_type}"
