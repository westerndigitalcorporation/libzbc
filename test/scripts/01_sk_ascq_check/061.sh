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

zbc_test_init $0 "READ across write-pointer zones (FULL->EMPTY)" $*

# Get drive information
zbc_test_get_device_info

zone_type=${test_zone_type:-"0x2|0x3"}

expected_sk="Illegal-request"
expected_asc="Read-boundary-violation"		# read cross-zone

if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone is of type ${zone_type} and EMPTY"
fi
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

if [ ${target_type} = "0x4" ]; then
    expected_asc="Attempt-to-read-invalid-data"	# because second zone has no data
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${target_size}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed, zone_type=${target_type}"

zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} $(( ${target_lba} + 1 ))
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial RESET_WP failed, zone_type=${target_type}"

zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2
zbc_test_get_sk_ascq

if [ ${unrestricted_read} -eq 1 -o ${target_type} = "0x3" ]; then
    # URSWRZ enabled or SWP zone -- expected to succeed
    zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
else
    # URSWRZ disabled and non-SWP write-pointer zone -- expected to fail
    zbc_test_check_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
rm -f ${zone_info_file}

