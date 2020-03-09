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

zbc_test_init $0 "READ access sequential zone LBAs after write pointer" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"

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
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} $((lblk_per_pblk+1))

# Check result
zbc_test_get_sk_ascq

if [ "${unrestricted_read}" = "1" -o ${device_model} = "Host-aware" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}

