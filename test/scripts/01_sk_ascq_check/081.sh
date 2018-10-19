#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "READ cross-zone FULL->EMPTY (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

# Search target LBA
zbc_test_get_wp_zones_cond_or_NA "FULL" "EMPTY"
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# because second zone has no data
if [[ ${target_type} == @(${ZT_RESTRICT_READ_XZONE}) ]]; then
    expected_asc="Read-boundary-violation"	# read cross-zone
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
if [[ ${unrestricted_read} -ne 0 || \
	${target_type} != @(${ZT_RESTRICT_READ_GE_WP}|${ZT_RESTRICT_READ_XZONE}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
fi
