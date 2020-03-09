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

zbc_test_init $0 "WRITE full zone" $*

# Expected error code for host-managed devices
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get drive information
zbc_test_get_device_info

# Set target zone type
if [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

# Get zone information
zbc_test_get_zone_info

# Search target zone
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"

# Start testing
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_slba}

zbc_test_run ${bin_path}/zbc_test_write_zone -v -n 1 ${device} ${target_slba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq

if [ ${device_model} = "Host-aware" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}

