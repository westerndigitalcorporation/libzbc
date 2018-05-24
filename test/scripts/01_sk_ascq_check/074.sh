#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

function write_check_inactive()
{
    local target_cond="$1"
    #XXX Emulator may check these zone conditions before boundary checks
    if [ -n "${CHECK_ZC_BEFORE_ZT}" ]; then
        if [ "${target_cond}" = "0xc" ]; then
	    expected_sk="Aborted-command"
	    expected_asc="Zone-is-inactive"
        elif [ "${target_cond}" = "0xf" ]; then
	    expected_sk="Medium-error"
	    expected_asc="Zone-is-offline"
        elif [ "${target_cond}" = "0xd" ]; then
	    expected_sk="Medium-error"
	    expected_asc="Zone-is-read-only"
        fi
    fi
}

zbc_test_init $0 "WRITE across zone-type spaces (cross-type boundary violation)" $*

# Set expected error code - ZBC 4.4.3.4.2 penultimate paragraph
expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"		# write cross-type

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search last CMR zone info
zbc_test_search_last_zone_vals_from_zone_type "0x1|0x4"

if [ $? -ne 0 -o $(( ${target_slba} + ${target_size} )) -gt ${max_lba} ]; then
    # CMR nonexistent or at top of LBA space -- try for last SMR instead
    zbc_test_search_last_zone_vals_from_zone_type "0x2|0x3"
fi

if [ $? -ne 0 ]; then
    # Most likely the test is broken somehow...
    zbc_test_print_not_applicable "Device has no zones of any valid type"
fi

boundary_lba=$(( ${target_slba} + ${target_size} ))	# first LBA after boundary
target_lba=$(( ${boundary_lba} - 1 ))			# last LBA before boundary

if [[ ${target_type} = @(0x2|0x4) ]]; then
    # Prepare SWR/WPC zone for the cross-type write -- get WP close to the boundary
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
    zbc_test_fail_if_sk_ascq "Initial RESET_WP failed, zone_type=${target_type}"

    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} $(( ${target_size} - 1 ))
    zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"
fi

if [ ${boundary_lba} -gt ${max_lba} ]; then
    # Boundary is at EOM
    expected_sk="Illegal-request"
    expected_asc="Logical-block-address-out-of-range"
else
    # Check the the zone just before the boundary for inactive, offline, or rdonly
    write_check_inactive ${target_cond}		# sets expected_* if so

    # Get info on the zone just after the boundary
    zbc_test_search_vals_from_slba ${boundary_lba}

    # Check the the zone just after the boundary for inactive, offline, or rdonly
    write_check_inactive ${target_cond}		# sets expected_* if so
fi

# Start testing
# Write across the boundary at the end of a zone-type in LBA space
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
rm -f ${zone_info_file}
