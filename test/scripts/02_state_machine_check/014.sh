#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016-2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

if [[ -n "${test_io_size}" && ${test_io_size} -eq 0 ]]; then
    # CLOSE transitions an OPEN zone to EMPTY if the WP points to the start of the zone
    end_state="EMPTY"
    expected_cond="${ZC_EMPTY}"
else
    end_state="CLOSED"
    expected_cond="${ZC_CLOSED}"
fi

zbc_test_init $0 "CLOSE_ZONE IMPLICIT OPEN to EXPLICIT OPEN to ${end_state}" $*

# Get drive information
zbc_test_get_device_info

write_size=${test_io_size:-${lblk_per_pblk}}

# Search target LBA
zbc_test_get_seq_zones_cond_or_NA "EMPTY"

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Change the zone state to Implicitly-Opened
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${write_size}
zbc_test_fail_exit_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"

# Change the zone state to Explicitly-Opened
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_slba}
zbc_test_fail_exit_if_sk_ascq "Zone OPEN failed, zone_type=${target_type}"

# Now try a close and check the resulting state
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_slba}

# Get SenseKey, ASC/ASCQ
zbc_test_get_sk_ascq

# Get target zone condition
zbc_test_get_target_zone_from_slba ${target_slba}

# Check result
zbc_test_check_zone_cond_wp $(( ${target_slba} + ${write_size} ))
