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

zbc_test_init $0 "RESET_WRITE_PTR implicit open to empty" $*

expected_cond="0x1"

# Get drive information
zbc_test_get_device_info

if [ -n "${test_zone_type}" ]; then
    zone_type=${test_zone_type}
else
    zone_type="0x2|0x3"
fi

if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone is of type ${zone_type} and EMPTY"
fi
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 8
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"

zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_lba}

# Get SenseKey, ASC/ASCQ
zbc_test_get_sk_ascq

# Get zone information
zbc_test_get_zone_info "1"

# Get target zone condition
zbc_test_search_vals_from_slba ${target_lba}

# Check result
zbc_test_check_zone_cond "zone_type=${target_type}"

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_lba}

rm -f ${zone_info_file}
