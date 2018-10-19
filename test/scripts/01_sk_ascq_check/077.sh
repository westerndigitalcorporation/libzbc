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

zbc_test_init $0 "WRITE across zone domain boundary" $*

# Set expected error code - ZBC 4.4.3.4.2 penultimate paragraph
expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"		# write cross-type

# Get drive information
zbc_test_get_device_info

# Search last non-sequential zone info
zbc_test_search_last_zone_vals_from_zone_type "${ZT_NON_SEQ}"

if [[ $? -ne 0 || $(( ${target_slba} + ${target_size} )) -gt ${max_lba} ]]; then
    # non-sequential nonexistent or at top of LBA space -- try for last sequential instead
    zbc_test_search_last_zone_vals_from_zone_type "${ZT_SEQ}"
    if [ $? -ne 0 ]; then
	zbc_test_print_not_applicable "Device has no sequential or non-sequential zones"
    fi
fi

boundary_lba=$(( ${target_slba} + ${target_size} ))	# first LBA after boundary
target_lba=$(( ${boundary_lba} - ${lblk_per_pblk} ))	# last block before boundary
target_lba_type=${target_type}

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${boundary_lba}

# Start testing
if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_GT_WP})
			&& ${target_cond} == @(${ZC_AVAIL}) ]]; then
    # First get the write pointer close to the boundary
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
    zbc_test_fail_exit_if_sk_ascq "Initial RESET_WP failed, zone_type=${target_type}"

    zbc_test_run ${bin_path}/zbc_test_write_zone -v \
			${device} ${target_slba} $(( ${target_size} - ${lblk_per_pblk} ))
    zbc_test_fail_exit_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"
fi

if [ ${boundary_lba} -gt ${max_lba} ]; then
    # Boundary is at End of Medium
    expected_sk="Illegal-request"
    expected_asc="Logical-block-address-out-of-range"
else
    # Check the the zone just before the boundary for availability
    zbc_write_check_available ${target_type} ${target_cond}	# sets expected_* if not
    ret=$?

    # Get info on the zone just after the boundary
    zbc_test_get_target_zone_from_slba ${boundary_lba}

    # If first zone was available, check the the zone just after the boundary
    if [ ${ret} -eq 0 ]; then
	zbc_write_check_available ${target_type} ${target_cond}	# sets expected_* if not
    fi
fi

# Write across the zone-type boundary
zbc_test_run ${bin_path}/zbc_test_write_zone -v \
			${device} ${target_lba} $(( ${lblk_per_pblk} * 2 ))

# Check result
zbc_test_get_sk_ascq
if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_XTYPE}) &&
      ${target_lba_type} != @(${ZT_DISALLOW_WRITE_XTYPE}) ]]; then
    # Neither zone type prohibits the boundary crossing
    zbc_test_check_no_sk_ascq "zone1_type=${target_lba_type} zone2_type=${target_type}"
else
    # One or both zone types prohibit the boundary crossing
    zbc_test_check_sk_ascq "zone1_type=${target_lba_type} zone2_type=${target_type}"
fi
