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

zbc_test_init $0 "WRITE across zone-type spaces (cross-type boundary violation)" $*

# Set expected error code - ZBC 4.4.3.4.2 penultimate paragraph
expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"		# write cross-type

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search last non-sequential zone info
zbc_test_search_last_zone_vals_from_zone_type "${ZT_NON_SEQ}"

if [ $? -ne 0 -o $(( ${target_slba} + ${target_size} )) -gt ${max_lba} ]; then
    # non-sequential nonexistent or at top of LBA space -- try for last sequential instead
    zbc_test_search_last_zone_vals_from_zone_type "${ZT_SEQ}"
fi

if [ $? -ne 0 ]; then
    # Most likely the test is broken...
    zbc_test_print_not_applicable "Device has no zones of any valid type"
fi

boundary_lba=$(( ${target_slba} + ${target_size} ))	# first LBA after boundary
target_lba=$(( ${boundary_lba} - ${sect_per_pblk} ))	# last block before boundary

# Start testing
if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_GT_WP}) ]]; then
    # Prepare zone for the cross-type write -- get WP close to the boundary
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
    zbc_test_fail_if_sk_ascq "Initial RESET_WP failed, zone_type=${target_type}"

    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} $(( ${target_size} - ${sect_per_pblk} ))
    zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"
fi

if [ ${boundary_lba} -gt ${max_lba} ]; then
    # Boundary is at End of Medium
    expected_sk="Illegal-request"
    expected_asc="Logical-block-address-out-of-range"
else
    # Check the the zone just before the boundary for availability
    write_check_available ${target_cond}		# sets expected_* if not

    # Get info on the zone just after the boundary
    zbc_test_search_vals_from_slba ${boundary_lba}

    # Check the the zone just after the boundary for availability
    write_check_available ${target_cond}		# sets expected_* if not
fi

# Write across the boundary at the end of a zone-type in LBA space
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} $(( ${sect_per_pblk} * 2 ))

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
rm -f ${zone_info_file}
