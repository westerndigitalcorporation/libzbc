#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "READ across write-pointer zones (FULL->FULL)" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Read-boundary-violation"		# read cross-zone

# Search target LBA
zbc_test_get_wp_zone_tuple_cond_or_NA "FULL" "FULL"
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq

if [[ ${unrestricted_read} -ne 0 || ${target_type} != @(${ZT_RESTRICT_READ_XZONE}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} $(( ${target_lba} + 1 ))
rm -f ${zone_info_file}
