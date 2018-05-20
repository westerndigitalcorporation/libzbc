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

zbc_test_init $0 "READ CMR/SMR zone type boundary violation (cross-type)" $*

# Set expected error code - ZBC 4.4.3.4.2 penultimate paragraph
expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# read cross-type

# Get drive information
zbc_test_get_device_info

if [ ${conv_zone} -eq 0 ]; then
	#XXX Crossing WPC->SEQ returns a different error as per bogosity in ZA-r4 SPEC
	expected_asc="Read-boundary-violation"	# read cross-type (XXX BOGUS SPEC)
fi

# Get zone information
zbc_test_get_zone_info

# Search first SMR zone info
zbc_test_search_vals_from_zone_type "0x2|0x3"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No SMR zone LBA space"
fi
reach_lba=${target_slba}

#XXX Current emulator checks these zone conditions before type
if [ -n ${CHECK_ZC_BEFORE_ZT} ]; then
	if [ "${target_cond}" = "0xc" ]; then
		expected_sk="Aborted-command"
		expected_asc="Zone-is-inactive"
	elif [ "${target_cond}" = "0xd" ]; then
		expected_sk="Aborted-command"
		expected_asc="Zone-is-read-only"
	elif [ "${target_cond}" = "0xf" ]; then
		expected_sk="Medium-error"
		expected_asc="Zone-is-offline"
	fi
fi

# Search last CMR zone info
zbc_test_search_last_zone_vals_from_zone_type "0x1|0x4"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No CMR zone LBA space"
fi

# Read from last CMR LBA to first SMR LBA
boundary_lba=$(( ${target_slba} + ${target_size} ))
target_lba=$(( ${boundary_lba} - 1 ))
_nlba=$(( ${reach_lba} - ${target_lba} + 1 ))

#XXX Current emulator checks these zone conditions before type
zbc_test_search_vals_from_slba ${boundary_lba}
if [ -n ${CHECK_ZC_BEFORE_ZT} ]; then
	if [ "${target_cond}" = "0xc" ]; then
		expected_sk="Aborted-command"
		expected_asc="Zone-is-inactive"
	elif [ "${target_cond}" = "0xd" ]; then
		expected_sk="Aborted-command"
		expected_asc="Zone-is-read-only"
	elif [ "${target_cond}" = "0xf" ]; then
		expected_sk="Medium-error"
		expected_asc="Zone-is-offline"
	fi
fi

# Start testing
# Read across the boundary from CMR LBA space into SMR LBA space
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} ${_nlba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
rm -f ${zone_info_file}

