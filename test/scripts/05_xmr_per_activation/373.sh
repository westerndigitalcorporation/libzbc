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

zbc_test_init $0 "WRITE a GAP zone" $*

# Get drive information
zbc_test_get_device_info

# Search target zone
zbc_test_search_gap_zone_or_NA

expected_sk="Illegal-request"
expected_asc="Attempt-to-access-GAP-zone"

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
