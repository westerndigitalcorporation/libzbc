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

zbc_test_init $0 "Run ZBC test on mixed CMR-SMR device" $*

# Set expected error code
expected_sk=""
expected_asc=""

# Get realm information
zbc_test_get_realm_info

# Convert roughly half of the realms to SMR -
# Find a CMR realm that is convertible to SMR
zbc_test_search_realm_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable
fi

# Find the total number of convertible realms
zbc_test_count_cvt_to_seq_realms
if [ $nr_cvt_to_seq_realms -eq 0 ]; then
    zbc_test_print_failed
fi

# Take the first half
nr=$[nr_cvt_to_seq_realms/2]
if [ $nr -eq 0 ]; then
    nr=$[nr + 1]
fi

# Convert the realms
zbc_test_run ${bin_path}/zbc_test_convert_realms -v ${device} ${realm_num} ${nr} "seq"

# Start ZBC test
zbc_test_meta_run ./zbc_test.sh -n ${device}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

