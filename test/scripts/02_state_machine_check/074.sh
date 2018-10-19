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

zbc_test_init $0 "WRITE ${test_io_size:-"one physical"} block(s) closed to implicit open" $*

expected_cond="${ZC_IOPEN}"

# Get drive information
zbc_test_get_device_info

# Search target LBA
zbc_test_get_seq_zones_cond_or_NA "CLOSEDL"
write_size=${test_io_size:-${lblk_per_pblk}}

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Write some more to the zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
					${target_ptr} ${write_size}
zbc_test_fail_exit_if_sk_ascq "WRITE failed, zone_type=${target_type}"

zbc_test_get_target_zone_from_slba ${target_slba}
zbc_test_check_zone_cond_wp $(( ${target_slba} + ${lblk_per_pblk} + ${write_size} ))
