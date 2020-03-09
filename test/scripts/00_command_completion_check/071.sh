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

zbc_test_init $0 "READ command completion (vector I/O)" $*

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

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_ignored_cond ${zone_type} "0xe"
target_lba=$(( ${target_ptr} ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v -vio 3 ${device} ${target_lba} ${lblk_per_pblk}
zbc_test_run ${bin_path}/zbc_test_read_zone -v -vio 3 ${device} ${target_lba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
rm -f ${zone_info_file}

# Check failed
zbc_test_check_failed

