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

zbc_test_init $0 "OPEN_ZONE ZONE-ID of disallowed zone-type ignored when ALL bit is set" $*

# Get drive information
zbc_test_get_device_info

# Search target LBA
zbc_test_search_non_seq_zone_cond_or_NA "${ZC_NON_FULL}"
expected_cond=${target_cond}

# Start testing
# Attempt OPEN ALL, specifying an LBA with a disallowed zone type, expected to be IGNORED
zbc_test_run ${bin_path}/zbc_test_open_zone --ALL ${device} ${target_slba}

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}
zbc_test_check_zone_cond "zone_type=${target_type}"
