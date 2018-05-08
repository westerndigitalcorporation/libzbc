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
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x0400"	# MAXDX

# Get drive information
zbc_test_get_device_info

if [ "${maxact_control}" == 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting MAXIMUM ACTIVATION"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a CMR domain that is convertible to SMR
zbc_test_search_domain_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain currently conventional is convertible to sequential"
fi

# Assume that all convertible domains are contiguous
zbc_test_count_cvt_to_seq_domains

# Set the maximum domains convertible too small for the number of zones
maxd=$(( ${nr_cvt_to_seq_domains} - 1 ))

# Lower the maximum number of domains to activate
zbc_test_run ${bin_path}/zbc_test_dev_control -q -maxd ${maxd} ${device}

# Make sure the command succeeded
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 ${device} ${domain_num} ${nr_cvt_to_seq_domains} "seq"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

# Post-process
zbc_test_run ${bin_path}/zbc_test_dev_control -q -maxd unlimited ${device}
