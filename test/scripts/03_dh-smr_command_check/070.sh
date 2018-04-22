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

zbc_test_init $0 "ZONE ACTIVATE(32) in excess of max_activate (domain addressing)" $*

# Set expected error code
expected_sk="Unknown-sense-key 0x00"
expected_asc="Unknown-additional-sense-code-qualifier 0x00"
expected_err_za="0x0004"
expected_err_cbf="0"

# Get drive information
zbc_test_get_device_info

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a CMR domain that is convertable to SMR
zbc_test_search_domain_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain currently conventional is convertible to sequential"
fi

# Assume that all convertible domains are contiguous
zbc_test_count_cvt_to_seq_domains

# Set the maximum domains convertible too small for the number of zones
maxd=$(( ${nr_cvt_to_seq_domains} - 1 ))

# Start testing
zbc_dev_control -maxd ${maxd} ${device} > /dev/null
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${domain_num} ${nr_cvt_to_seq_domains} "seq"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

# Post-process
zbc_dev_control -maxd unlimited ${device} > /dev/null
