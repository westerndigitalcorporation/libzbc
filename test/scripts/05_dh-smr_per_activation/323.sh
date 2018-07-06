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

zbc_test_init $0 "OPEN an OFFLINE zone" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Data-protect"
expected_asc="Zone-is-offline"

# Search target zone
zbc_test_search_zone_cond_or_NA ${ZC_OFFLINE}

# Start testing
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_slba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq "zone_type=${target_type}"

# Post process
zbc_test_check_failed
rm -f ${zone_info_file}
