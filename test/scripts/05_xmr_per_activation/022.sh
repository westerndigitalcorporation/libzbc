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

zbc_test_init $0 "REPORT REALMS: realm locator above max LBA" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Logical-block-address-out-of-range"

# Get drive information
zbc_test_get_device_info

# One above the zoned maximum address
realm_locator=$(( ${max_lba} + 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_report_realms -v -start ${realm_locator} ${device}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
