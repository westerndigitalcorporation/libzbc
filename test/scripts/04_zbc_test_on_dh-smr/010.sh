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
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "Run ZBC test on all-CMR device" $*

# Set expected error code
expected_sk=""
expected_asc=""

#zbc_reset_test_device

# Start ZBC test
zbc_test_meta_run ./zbc_test.sh -n ${device}
if [ $? -ne 0 ]; then
    sk="fail"
    asc="ZBC test failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

