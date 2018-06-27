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

zbc_test_init $0 "READ zero blocks exactly at End of Medium" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Logical-block-address-out-of-range"

# Get drive information
zbc_test_get_device_info

target_lba=$(( ${max_lba} + 1 ))

# Start testing
# Attempt to read zero blocks at the first LBA beyond End of Medium
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 0

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
