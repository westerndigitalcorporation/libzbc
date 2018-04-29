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

zbc_test_init $0 "Run ZBC test on another mixed CMR-SMR device" $*

export ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH}/mix2

arg_b=""
if [ ${batch_mode} -ne 0 ] ; then
	arg_b="-b"
fi

# Set expected error code
expected_sk=""
expected_asc=""

zbc_test_reset_device

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find the total number of convertible domains
zbc_test_count_cvt_domains		# nr_domains

# Configure the conversion domains, with all domains freshly converted except 0 and 5.
# This ends up with all conversion domains SWR except domains 0-10 and the last domain.

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 "conv"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration (1 4 conv)"
	exit 1
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 $(( ${nr_domains} - 6 )) "conv"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration (6 $(( ${nr_domains} - 6 )) conv)"
	exit 1
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 "seq"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration (1 4 seq)"
	exit 1
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 $(( ${nr_domains} - 6 )) "seq"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration (6 $(( ${nr_domains} - 6 )) seq)"
	exit 1
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 "conv"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration (1 4 conv)"
	exit 1
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 5 "conv"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration (6 5 conv)"
	exit 1
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} $(( ${nr_domains} - 1 )) 1 "conv"
if [ $? != 0 ]; then
	echo "Failed to convert device to intended test configuration ($(( ${nr_domains} - 1 )) 1 conv)"
	exit 1
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
zbc_test_reset_device
