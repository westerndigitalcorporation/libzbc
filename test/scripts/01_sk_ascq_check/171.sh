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

zbc_test_init $0 "WRITE cross-zone ${zone_cond_1}->${zone_cond_2} and ending below WP" $*

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

expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"		# SWR write cross-zone

# Compute the last LBA of the first zone
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
# Write across the zone boundary, stopping below the WP of the second zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
if [[ ${target_type} != @(0x2) ]]; then
    zbc_test_check_no_sk_ascq zone_type=${target_type}
else
    zbc_test_check_sk_ascq zone_type=${target_type}
fi

# Post process
rm -f ${zone_info_file}
