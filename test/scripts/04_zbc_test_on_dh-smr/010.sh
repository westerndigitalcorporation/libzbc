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

zbc_test_init $0 "Run ZBC test on all-CMR device" $*

export ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH}/04.010_allcmr

zbc_test_reset_device

zbc_test_get_device_info

# Pass the batch_mode flag through to the run we invoke below
arg_b=""
if [ ${batch_mode} -ne 0 ] ; then
	arg_b="-b"
fi

# Start ZBC test
zbc_test_meta_run ./zbc_test.sh ${arg_b} -n ${device}
if [ $? -ne 0 ]; then
    sk="fail"
    asc="ZBC test failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

# Post-process cleanup
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
