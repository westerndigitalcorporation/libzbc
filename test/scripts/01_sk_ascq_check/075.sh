#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE with unaligned ending physical block (type=${test_zone_type:-${ZT_SEQ}})" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# ending physical unaligned

# Get drive information
zbc_test_get_device_info

# if physical block size == logical block size then this failure cannot occur
if [ ${physical_block_size} -eq ${logical_block_size} ]; then
    zbc_test_print_not_applicable \
	"physical_block_size=logical_block_size (${logical_block_size} B)"
fi

# Search target LBA
zbc_test_search_zone_cond_or_NA "${ZC_EMPTY}|${ZC_NOT_WP}"
initial_wp=${target_ptr}
expected_cond="${target_cond}"

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} 1

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}

if [[ ${target_type} == @(${ZT_CONV}) ]]; then
    # Don't check WP on conventional zones
    zbc_test_check_zone_cond "zone_type=${target_type}"
elif [[ ${target_type} != @(${ZT_REQUIRE_WRITE_PHYSALIGN}) ]]; then
    # Write should have succeeded -- check that the WP has updated
    zbc_test_check_zone_cond_wp $(( ${target_slba} + 1 )) "zone_type=${target_type}"
else
    # Write should have failed -- check WP is unmodified
    zbc_test_check_sk_ascq_zone_cond_wp ${initial_wp} "zone_type=${target_type}"
fi
