#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE unaligned crossing write pointer" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting below and ending above WP

# Search target LBA
zbc_test_get_wp_zone_or_NA "${ZC_NON_FULL}"
target_lba=${target_ptr}

# Start testing
# Write a block starting at the write pointer
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}
if [ $? -ne 0 ]; then
    printf "\nInitial write failed"
else
    # Attempt to write more blocks to the same starting LBA, overwriting the ones just written
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} $(( ${lblk_per_pblk} * 2 ))
fi

# Check result
zbc_test_get_sk_ascq

if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_LT_WP}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi

# Post process
rm -f ${zone_info_file}
