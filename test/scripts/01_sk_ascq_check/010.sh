#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

. ../zbc_test_lib.sh

zbc_test_init $0 $*

zbc_test_info "REPORT_ZONES logical block out of range..."

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Logical-block-address-out-of-range"

# Get drive information
zbc_test_get_device_info

# Set target LBA
target_lba=$(( ${max_lba} + 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_report_zones -v -lba ${target_lba} ${device}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
