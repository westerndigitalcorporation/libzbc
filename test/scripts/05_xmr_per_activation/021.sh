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

zbc_test_init $0 "REPORT REALMS: realm locator at max LBA" $*

# Get drive information
zbc_test_get_device_info

# Setting realm locator equal to zoned maximum address is OK
realm_locator=${max_lba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_report_realms -v -start ${realm_locator} ${device}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq
