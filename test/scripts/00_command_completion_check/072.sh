#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2019, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE command completion, vector I/O (type=${test_zone_type:-${ZT_SEQ}})" $*

# Set expected error code
expected_sk=""
expected_asc=""

# Get drive information
zbc_test_get_device_info

if [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

# Search target LBA
zbc_test_search_zone_cond_or_NA "${ZC_EMPTY}|${ZC_NOT_WP}"
if [[ $target_cond != $ZC_NOT_WP ]]; then
	target_lba=$(( ${target_ptr} ))

	# Specify post-processing to occur when script exits
	zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_lba}
else
	target_lba=$(( ${target_slba} ))
fi

# Start testing, write a specific byte pattern
zbc_test_run ${bin_path}/zbc_test_write_zone -v -vio 8 -p 0xaa ${device} ${target_lba} ${lblk_per_pblk}

# Check result
zbc_test_fail_exit_if_sk_ascq "WRITE failed, zone_type=${target_type}, lba=${target_lba}"

# Make sure we read back the same byte pattern
zbc_test_run ${bin_path}/zbc_test_read_zone -v -vio 8 -p 0xaa ${device} ${target_lba} ${lblk_per_pblk}

#Check the result again. The read will fail if there is a mismatch
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq
