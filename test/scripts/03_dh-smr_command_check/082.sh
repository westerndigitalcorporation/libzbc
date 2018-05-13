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

zbc_test_init $0 "ZONE ACTIVATE(16): all Conventional to SWP (domain addressing)" $*

# Set expected error code
expected_sk=""
expected_asc=""

# Get drive information
zbc_test_get_device_info

if [ "${seq_pref_zone}" == 0 ]; then
    zbc_test_print_not_applicable "Device does not support conversion to SWP zone type"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a conventional domain that is convertable to SMR
zbc_test_search_domain_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently conventional and convertible to sequential"
fi

# Assume that all convertable domains are contiguious
zbc_test_count_cvt_domains
zbc_test_count_cvt_to_seq_domains
if [ $(expr "${domain_num}" + "${nr_cvt_to_seq_domains}") -gt ${nr_domains} ]; then
    nr_cvt_to_seq_domains=$(expr "${nr_domains}" - "${domain_num}")
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${domain_num} ${nr_cvt_to_seq_domains} "seqp"

# Check result
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that no convertable conventional domains present
    zbc_test_get_cvt_domain_info
    zbc_test_search_domain_by_type_and_cvt "1" "seq"
    if [ $? -eq 0 ]; then
	sk=${domain_num}
	expected_sk="no-conv-to-seq"
    fi
fi

# Check failed
zbc_test_check_no_sk_ascq
zbc_test_check_failed
