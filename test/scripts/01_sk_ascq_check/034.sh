#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "CLOSE_ZONE ZONE-ID of disallowed zone type ignored when ALL bit is set" $*

# Get drive information
zbc_test_get_device_info

# Search target LBA
zbc_test_search_non_seq_zone_cond_or_NA "${ZC_NON_FULL}"
expected_cond=${target_cond}

# Start testing
# Attempt CLOSE ALL, specifying an LBA with a disallowed zone type, expected to be IGNORED
zbc_test_run ${bin_path}/zbc_test_close_zone --ALL ${device} ${target_slba}

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}
zbc_test_check_zone_cond "zone_type=${target_type}"
