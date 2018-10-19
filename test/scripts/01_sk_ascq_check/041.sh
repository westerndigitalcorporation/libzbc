#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016-2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "FINISH_ZONE disallowed zone type" $*

expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get drive information
zbc_test_get_device_info

# Search target LBA
test_zone_type=${ZT_CONV}
zbc_test_search_zone_cond_or_NA "${ZC_NOT_WP}"

# Start testing
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_slba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq "zone_type=${target_type}"
