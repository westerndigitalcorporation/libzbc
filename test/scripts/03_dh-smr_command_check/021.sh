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

zbc_test_init $0 "MEDIA_CONVERT conversion to CMR" $*

# Set expected error code
expected_sk=""
expected_asc=""

# Get drive information
zbc_test_get_device_info

# Get range information
zbc_test_get_cvt_range_info

# Find an SMR rrange that is convertable to CMR
zbc_test_search_range_by_type_and_cvt "2" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_media_convert -v ${device} ${range_num} 1 "conv"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that the range is converted
    zbc_test_get_cvt_range_info
    zbc_test_search_cvt_range_by_number ${range_num}
    if [ $? -ne 0 -o "${range_type}" != "0x1" ]; then
        sk=${range_type}
        expected_sk="0x2"
        zbc_test_print_failed_sk
    fi
fi

# Check failed
zbc_test_check_failed

