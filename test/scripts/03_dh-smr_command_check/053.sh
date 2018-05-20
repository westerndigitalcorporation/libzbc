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

zbc_test_init $0 "ZONE ACTIVATE(32): all SMR to CMR (domain addressing)" $*

# Get drive information
zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${wpc_zone} -ne 0 ]; then
    cmr_type="wpc"
else
    zbc_test_print_not_applicable "Neither conventional nor WPC zones are supported by the device"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find the first SMR domain that is convertible to CMR
zbc_test_search_domain_by_type_and_cvt "0x2|0x3" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently SMR and convertible to CMR"
fi

# Assume that all convertible domains are contiguous
zbc_test_count_cvt_domains
zbc_test_count_cvt_to_conv_domains
if [ $(expr "${domain_num}" + "${nr_cvt_to_conv_domains}") -gt ${nr_domains} ]; then
    nr_cvt_to_conv_domains=$(expr "${nr_domains}" - "${domain_num}")
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 ${device} ${domain_num} ${nr_cvt_to_conv_domains} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "ACTIVATE failed to cmr_type=${cmr_type}"

if [ -z "${sk}" ]; then
    # Verify that no convertible SMR domain is present
    zbc_test_get_cvt_domain_info
    zbc_test_search_domain_by_type_and_cvt "0x2|0x3" "conv"
    if [ $? -eq 0 ]; then
        sk=${domain_num}
        expected_sk="no-seq-to-conv"
    fi
fi

# Check failed
zbc_test_check_no_sk_ascq
zbc_test_check_failed

