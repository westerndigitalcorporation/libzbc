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

zbc_test_init $0 "CLOSE_ZONE conventional zone" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type "0x1"
func_ret=$?

if [ ${func_ret} -gt 0 ]; then
    zbc_test_print_not_applicable
fi

target_lba=$(( ${target_slba} ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq


# Post process
rm -f ${zone_info_file}

