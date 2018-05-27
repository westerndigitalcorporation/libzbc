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

zone_cond_1=FULL
zone_cond_2=OPEN

zbc_test_init $0 "READ cross-zone ${zone_cond_1}->${zone_cond_2} and ending above WP" $*

# Get drive information
zbc_test_get_device_info

zone_type=${test_zone_type:-"0x2|0x3"}
if [ ${zone_type} == 0x1 ]; then
    zbc_test_print_not_applicable "Requested test type is ${zone_type} but test requires a write-pointer zone"
fi

# Get zone information
zbc_test_get_zone_info

# Get a pair of zones
zbc_test_zone_tuple_cond ${zone_type} ${zone_cond_1} ${zone_cond_2}
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No zone pairs ${zone_type} ${zone_cond_1} ${zone_cond_2}"
fi

if [ ${unrestricted_read} -eq 0 ]; then
    if [ ${target_type} = "0x2" ]; then
	expected_sk="Illegal-request"
	expected_asc="Read-boundary-violation"		# SWR read cross-zone
    elif [ ${target_type} = "0x4" ]; then
	expected_sk="Illegal-request"
	expected_asc="Attempt-to-read-invalid-data"	# WPC Read ending above WP
    fi
fi

# Compute the last LBA of the first zone
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
# Read across the zone boundary and beyond the WP of the second zone
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 16

# Check result
zbc_test_get_sk_ascq
if [[ ${unrestricted_read} -ne 0 || ${target_type} != @(0x2|0x4) ]]; then
    zbc_test_check_no_sk_ascq zone_type=${target_type} URSWRZ=${unrestricted_read}
else
    zbc_test_check_sk_ascq zone_type=${target_type} URSWRZ=${unrestricted_read}
fi

# Post process
rm -f ${zone_info_file}
