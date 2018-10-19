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

zbc_test_init $0 "WRITE cross-zone OPEN->EMPTY starting at Write Pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"		# write cross-zone

# Get a pair of zones we can try to write across their boundary
zbc_test_get_wp_zones_cond_or_NA "IOPENH" "EMPTY"
initial_wp=${target_ptr}
expected_cond="${target_cond}"
target_lba=$(( ${target_slba} + ${target_size} - ${lblk_per_pblk} ))

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone \
			${device} ${target_slba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone \
			${device} $(( ${target_slba} + ${target_size} ))

# Start testing
# Attempt to write through the remaining LBA of the zone and cross over into the next zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			${target_lba} $(( ${lblk_per_pblk} * 2 ))

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}

if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    # Write should have succeeded -- check WP of the second zone has updated
    zbc_test_get_target_zone_from_slba $(( ${target_slba} + ${target_size} ))
    zbc_test_check_zone_cond_wp $(( ${target_slba} + ${lblk_per_pblk} )) \
			"zone_type=${target_type}"
else
    # Write should have failed -- check WP of the first zone is unmodified
    zbc_test_check_sk_ascq_zone_cond_wp ${initial_wp} "zone_type=${target_type}"
fi
