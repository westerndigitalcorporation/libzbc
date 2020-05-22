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

zbc_test_init $0 "WRITE cross-zone EMPTY->EMPTY starting above Write Pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

# Get a pair of zones
zbc_test_get_wp_zones_cond_or_NA "EMPTY" "EMPTY"

if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    expected_sk="Illegal-request"
    expected_asc="Write-boundary-violation"	# Write cross-zone
    alt_expected_sk="Illegal-request"
    alt_expected_asc="Unaligned-write-command"	# Write starting above WP
else
    expected_sk="Illegal-request"
    expected_asc="Unaligned-write-command"	# Write starting above WP
fi

# Compute the last block of the first zone
target_lba=$(( ${target_slba} + ${target_size} - ${lblk_per_pblk} ))

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} \
			$(( ${target_slba} + ${target_size} ))

# Start testing
# Write across the zone boundary starting above the WP of the first zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			${target_lba} $(( ${lblk_per_pblk} * 2 ))

# Check result
zbc_test_get_sk_ascq
if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_GT_WP}|${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi
