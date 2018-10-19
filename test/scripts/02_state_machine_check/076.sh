#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE ${test_io_size:-"one physical"} block(s) explicit open to explicit open" $*

expected_cond="${ZC_EOPEN}"

# Get drive information
zbc_test_get_device_info

zbc_test_search_wp_zone_cond_or_NA ${ZC_AVAIL}
zbc_test_get_wp_zones_cond_or_NA "EOPEN"
write_size=${test_io_size:-${lblk_per_pblk}}

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Write part of the zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${write_size}
zbc_test_fail_exit_if_sk_ascq "WRITE failed, zone_type=${target_type}"

# Get target zone condition
zbc_test_get_target_zone_from_slba ${target_slba}

# Check result
zbc_test_check_zone_cond_wp $(( ${target_slba} + ${write_size} ))
