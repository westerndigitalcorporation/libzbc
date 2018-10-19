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

zbc_test_init $0 "WRITE ${test_io_size:-"one logical"} block(s) unaligned ending below write pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

write_size=${test_io_size:-1}

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting and ending below WP

# Search target LBA
zbc_test_get_wp_zones_cond_or_NA "IOPENL"	# get a zone and write the first block
initial_wp=${target_ptr}
expected_cond="${target_cond}"

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Attempt to write one of the same LBA again
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${write_size}

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}

if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_LT_WP}) ]]; then
    zbc_test_check_zone_cond_wp ${initial_wp} "zone_type=${target_type}"
else
    # Write should have failed -- check WP is unmodified
    zbc_test_check_sk_ascq_zone_cond_wp ${initial_wp} "zone_type=${target_type}"
fi
