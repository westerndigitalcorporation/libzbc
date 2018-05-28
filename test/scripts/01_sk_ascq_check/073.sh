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

zbc_test_init $0 "WRITE full zone" $*
# Also checks that writes to FULL zones succeed on SWP and WPC zones

# Get drive information
zbc_test_get_device_info

zone_type=${test_zone_type:-"0x2|0x3"}
if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"		# write full zone

# Get zone information
zbc_test_get_zone_info

# Search target zone
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone is of type ${zone_type} and EMPTY"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${target_size}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed, zone_type=${target_type}"

zbc_test_run ${bin_path}/zbc_test_write_zone -v -n 1 ${device} ${target_slba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq

if [[ ${target_type} == @(0x3|0x4) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
rm -f ${zone_info_file}

