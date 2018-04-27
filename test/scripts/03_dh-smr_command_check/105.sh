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

zbc_test_init $0 "ZONE ACTIVATE(32) LBA not domain-aligned (zone addressing)" $*

# Set expected error code
expected_sk="Unknown-sense-key 0x00"
expected_asc="Unknown-additional-sense-code-qualifier 0x00"

# Get information
zbc_test_get_device_info
zbc_test_get_zone_info
zbc_test_get_cvt_domain_info

# Find a CMR domain that is convertable to SMR
zbc_test_search_domain_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain currently conventional is convertible to sequential"
fi

# Add one zone-size to the starting zone to domain-misalign it for the test
zbc_test_search_vals_from_slba ${domain_conv_start}


echo "${domain_conv_start}" + "${target_size}"

start_lba=$(expr "${domain_conv_start}" + "${target_size}")
expected_err_za="0x4001"	# CBI | Zone Boundary Violation
expected_err_cbf="${start_lba}"

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${start_lba} ${domain_conv_len} "seq"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed
