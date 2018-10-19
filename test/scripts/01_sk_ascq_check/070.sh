#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE ${test_io_size:-"one physical"} block(s) unaligned starting above write pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting and ending above WP

# Search target LBA
zbc_test_search_wp_zone_cond_or_NA "${ZC_NON_FULL}"
target_lba=$(( ${target_ptr} + ${lblk_per_pblk} ))	# unaligned write starting above WP
initial_wp=${target_ptr}
expected_cond=${target_cond}

write_size=${test_io_size:-${lblk_per_pblk}}

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${write_size}

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}

if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_GT_WP}) ]]; then
    # Don't predict cond or WP after SWP non-sequential write
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    # Write should have failed -- check WP of the first zone is unmodified
    zbc_test_check_sk_ascq_zone_cond_wp ${initial_wp} "zone_type=${target_type}"
fi
