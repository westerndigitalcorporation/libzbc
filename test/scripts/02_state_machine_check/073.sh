#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE implicit open to full" $*

expected_cond="0xe"

# Get drive information
zbc_test_get_device_info

# Search target LBA
zbc_test_get_wp_zone_or_NA ${ZC_EMPTY}
target_lba=${target_slba}

# Start testing
# Write the first block of the zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"

if [ -z "${sk}" ]; then
    # Write the rest of the zone
    resid=$(( ${target_size} - ${lblk_per_pblk} ))
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} $(( ${target_lba} + ${lblk_per_pblk} )) ${resid}
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "WRITE failed, zone_type=${target_type}"

    if [ -z "${sk}" ]; then
	zbc_test_get_zone_info
	zbc_test_get_target_zone_from_slba ${target_lba}
	zbc_test_check_zone_cond
    fi
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_lba}

rm -f ${zone_info_file}
