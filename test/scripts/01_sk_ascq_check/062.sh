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

zbc_test_init $0 "READ CMR zone space end boundary violation (cross-type)" $*

# Set expected error code - ZBC 4.4.3.4.2 penultimate paragraph
expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# read cross-type

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search last CMR zone info
zbc_test_search_last_zone_vals_from_zone_type "0x1|0x4"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No CMR zone LBA space"
fi

boundary_lba=$(( ${target_slba} + ${target_size} ))

if [ ${boundary_lba} -gt ${max_lba} ]; then
    # CMR at top -- try for a SMR -> CMR crossing
    zbc_test_search_last_zone_vals_from_zone_type "0x2|0x3"
    if [ $? -eq 0 ]; then
	 boundary_lba=$(( ${target_slba} + ${target_size} ))
    fi
fi

target_lba=$(( ${boundary_lba} - 1 ))

#XXX Current emulator checks these zone conditions before boundary checks
if [ -n "${CHECK_ZC_BEFORE_ZT}" ]; then
    if [ "${target_cond}" = "0xc" ]; then
	expected_sk="Aborted-command"
	expected_asc="Zone-is-inactive"
    elif [ "${target_cond}" = "0xf" ]; then
	expected_sk="Medium-error"
	expected_asc="Zone-is-offline"
    fi
fi

if [ ${boundary_lba} -gt ${max_lba} ]; then
    # Boundary is EOM
    expected_sk="Illegal-request"
    expected_asc="Logical-block-address-out-of-range"
else
    zbc_test_search_vals_from_slba ${boundary_lba}

    #XXX Current emulator checks these zone conditions before boundary checks
    if [ -n "${CHECK_ZC_BEFORE_ZT}" ]; then
        if [ "${target_cond}" = "0xc" ]; then
	    expected_sk="Aborted-command"
	    expected_asc="Zone-is-inactive"
        elif [ "${target_cond}" = "0xf" ]; then
	    expected_sk="Medium-error"
	    expected_asc="Zone-is-offline"
        fi
    fi

fi

# Start testing
# Read across the boundary between CMR LBA space and whatever follows it in LBA space
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
#rm -f ${zone_info_file}
