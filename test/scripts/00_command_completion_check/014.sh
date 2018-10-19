#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 20162020, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "REPORT_ZONES (reporting option 0x3F) command completion" $*

# Get drive information
zbc_test_get_device_info

# Set target LBA
target_lba=0

# Set reporting option
reporting_option="63"

# Start testing
zbc_test_run ${bin_path}/zbc_test_report_zones -v -ro ${reporting_option} \
			-lba ${target_lba} ${device}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq
