#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zone_cond_1=FULL
zone_cond_2=IOPENL

zbc_test_init $0 "WRITE cross-zone ${zone_cond_1}->${zone_cond_2} and ending above Write Pointer" $*

# Get drive information
zbc_test_get_device_info

# Get a pair of zones
zbc_test_get_wp_zone_tuple_cond_or_NA ${zone_cond_1} ${zone_cond_2}

if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    expected_sk="Illegal-request"
    expected_asc="Write-boundary-violation"   		# write cross-zone
    if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_FULL}) ]]; then
	alt_expected_sk="Illegal-request"
	alt_expected_asc="Invalid-field-in-cdb"		# write starting in full zone
    fi
elif [[ ${target_type} == @(${ZT_DISALLOW_WRITE_FULL}) ]]; then
    expected_sk="Illegal-request"
    expected_asc="Invalid-field-in-cdb"			# write starting in full zone
fi

# Compute the last LBA of the first zone
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
# Write across the zone boundary and beyond the WP of the second zone, ending physaligned
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} $(( ${lblk_per_pblk} * 2 + 1 ))

# Check result
zbc_test_get_sk_ascq
if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_FULL}|${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi

# Post process
rm -f ${zone_info_file}
