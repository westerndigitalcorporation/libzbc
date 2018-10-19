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

if [[ -n "${test_io_size}" && ${test_io_size} -eq 0 ]]; then
    # CLOSE transitions an OPEN zone to EMPTY if the WP points to the start of the zone
    end_state="EMPTY"
    expected_cond="${ZC_EMPTY}"
else
    end_state="CLOSED"
    expected_cond="${ZC_CLOSED}"
fi

zbc_test_init $0 "CLOSE_ZONE IMPLICIT OPEN to ${end_state} (ALL bit set) (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

zbc_test_search_wp_zone_cond_or_NA ${ZC_EMPTY}
write_size=${test_io_size:-${lblk_per_pblk}}

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Make the zone Implicitly-Open
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${write_size}
zbc_test_fail_exit_if_sk_ascq \
    "Initial WRITE of ${write_size} block(s) failed, zone_type=${target_type}"

if [[ ${target_type} != @(${ZT_SEQ}) ]]; then
    expected_cond="${ZC_IOPEN}"		# unchanged
fi

zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} -1

# Get SenseKey, ASC/ASCQ
zbc_test_get_sk_ascq

zbc_test_get_target_zone_from_slba ${target_slba}

# Check result
if [[ ${target_type} == @(${ZT_SEQ}) ]]; then
    zbc_test_check_zone_cond_wp $(( ${target_slba} + ${write_size} )) \
	"type=${target_type} cond=${target_cond} write_size=${write_size}"
else
    zbc_test_check_sk_ascq_zone_cond_wp $(( ${target_slba} + ${write_size} )) \
	"type=${target_type} cond=${target_cond} write_size=${write_size}"
fi
