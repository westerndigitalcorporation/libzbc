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

zbc_test_init $0 "WRITE implicit open to implicit open" $*

expected_cond="0x2"

# Get drive information
zbc_test_get_device_info

zone_type=${test_zone_type:-"0x2|0x3"}
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

target_lba=${target_slba}

# Start testing
# Write the first few blocks of the zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 5
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"

if [ -z "${sk}" ]; then
    # Write more blocks in the zone

    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} $(( ${target_lba} + 5 )) 3
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "WRITE failed, zone_type=${target_type}"

    if [ -z "${sk}" ]; then
        zbc_test_get_zone_info
        zbc_test_search_vals_from_slba ${target_lba}
        zbc_test_check_zone_cond
    fi
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_lba}

rm -f ${zone_info_file}
