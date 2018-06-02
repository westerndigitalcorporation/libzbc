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

zone_cond_1=OPEN
zone_cond_2=FULL

zbc_test_init $0 "READ cross-zone ${zone_cond_1}->${zone_cond_2} starting above Write Pointer" $*

# Get drive information
zbc_test_get_device_info

# Get a pair of zones
zbc_test_get_wp_zone_tuple_cond_or_NA ${zone_cond_1} ${zone_cond_2}

expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# because second zone has no data
if [[ ${unrestricted_read} -eq 0 && ${target_type} == @(${ZT_RESTRICT_READ_XZONE}) ]]; then
    expected_asc="Read-boundary-violation"	# read cross-zone
fi

# Compute the last LBA of the first zone
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
# Read across the zone boundary starting above the WP of the first zone
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
if [[ ${unrestricted_read} -ne 0 || \
	${target_type} != @(${ZT_RESTRICT_READ_GE_WP}|${ZT_RESTRICT_READ_XZONE}) ]]; then
    zbc_test_check_no_sk_ascq zone_type=${target_type} URSWRZ=${unrestricted_read}
else
    zbc_test_check_sk_ascq zone_type=${target_type} URSWRZ=${unrestricted_read}
fi

# Post process
rm -f ${zone_info_file}
