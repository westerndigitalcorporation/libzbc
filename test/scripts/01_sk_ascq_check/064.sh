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

zbc_test_init $0 "READ ${test_io_size:-"one physical"} block(s) starting above write pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

read_size=${test_io_size:-${lblk_per_pblk}}

expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# read across WP

# Search target LBA
zbc_test_search_wp_zone_cond_or_NA "${ZC_NON_FULL}"

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Write a block starting at the write pointer
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_ptr} ${lblk_per_pblk}
if [ $? -ne 0 ]; then
    zbc_test_fail_exit "Initial write"
fi

# Attempt to read an LBA starting beyond the write pointer
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} \
		$(( ${target_ptr} + ${lblk_per_pblk} )) ${read_size}

# Check result
zbc_test_get_sk_ascq
if [[ ${unrestricted_read} -ne 0 || ${target_type} != @(${ZT_RESTRICT_READ_GE_WP}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
fi
