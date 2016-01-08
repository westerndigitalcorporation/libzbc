#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc.  All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

. ../zbc_test_lib.sh

zbc_test_init $0 $*

zbc_test_info "REPORT_ZONES (reporting option 0x10) command completion..."

# Set expected error code
expected_sk=""
expected_asc=""

# Get drive information
zbc_test_get_drive_info

# Set target LBA
target_lba="0"

# Set reporting option
reporting_option="16"

# Start testing
zbc_test_run ${bin_path}/zbc_test_report_zones -v -ro ${reporting_option} -lba ${target_lba} ${device}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

