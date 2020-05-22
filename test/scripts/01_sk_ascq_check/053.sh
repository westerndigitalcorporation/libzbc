#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2023, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "RESET_WRITE_PTR ZONE-ID > max_lba ignored when ALL bit is set" $*

# Get drive information
zbc_test_get_device_info

zbc_test_search_seq_zone_cond_or_NA ${ZC_EMPTY}

expected_cond="${ZC_EMPTY}"

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Make the zone non-empty
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}

# Attempt RESET ALL, specifying a bad LBA which is expected to be IGNORED
zbc_test_run ${bin_path}/zbc_test_reset_zone --ALL ${device} $(( ${max_lba} + 2 ))

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}
zbc_test_check_zone_cond "zone_type=${target_type}"
