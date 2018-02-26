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

zbc_test_init $0 "Convert a non-convertible CMR range to SMR" $*

# Set expected error code
expected_sk="Aborted-command"
expected_asc="Conversion-type-unsupported"

# Get drive information
zbc_test_get_device_info

# Get conversion range information
zbc_test_get_cvt_range_info

# Find the first SMR range that is not convertible to SMR
zbc_test_search_range_by_type_and_cvt "1" "noseq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_convert_realms -v ${device} ${range_num} 1 "seq"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Check failed
zbc_test_check_failed

