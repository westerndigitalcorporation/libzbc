#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE cross-zone FULL->OPEN and ending above Write Pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

# Get a pair of zones
zbc_test_get_wp_zones_cond_or_NA "FULL" "IOPENL"

if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    expected_sk="Illegal-request"
    expected_asc="Write-boundary-violation"		# write cross-zone
    if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_FULL}) ]]; then
	alt_expected_sk="Illegal-request"
	alt_expected_asc="Invalid-field-in-cdb"		# write starting in full zone
    fi
elif [[ ${target_type} == @(${ZT_DISALLOW_WRITE_FULL}) ]]; then
    expected_sk="Illegal-request"
    alt_expected_asc="Invalid-field-in-cdb"		# write starting in full zone
fi

# Compute the start of the last physical block of the first zone
target_lba=$(( ${target_slba} + ${target_size} - ${lblk_per_pblk} ))

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} \
			$(( ${target_slba} + ${target_size} ))

# Start testing
# Write across the zone boundary and beyond the WP of the second zone, ending physaligned
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			${target_lba} $(( ${lblk_per_pblk} * 3 ))

# Check result
zbc_test_get_sk_ascq
if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_XZONE}) &&
      ${target_type} != @(${ZT_DISALLOW_WRITE_FULL}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi
