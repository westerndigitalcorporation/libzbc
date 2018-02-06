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

zbc_test_init $0 "CONVERT_REALMS all realms to SMR" $*

# Set expected error code
expected_sk=""
expected_asc=""

# Get drive information
zbc_test_get_device_info

# Get realm information
zbc_test_get_realm_info

# Find a CMR realm that is convertable to SMR
zbc_test_search_realm_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable
fi

# Assume that all convertable realms are contiguious
zbc_test_count_cvt_to_seq_realms

# Start testing
zbc_test_run ${bin_path}/zbc_test_convert_realms -v ${device} ${realm_num} ${nr_cvt_to_seq_realms} "seq"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Verify that no convertable conventional realms present
zbc_test_get_realm_info
zbc_test_search_realm_by_type_and_cvt "1" "seq"
if [ $? -eq 0 ]; then
    sk=${realm_num}
    expected_sk=""
    zbc_test_print_failed_sk
fi

# Check failed
zbc_test_check_failed

