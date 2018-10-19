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
