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

zbc_test_init $0 "ZONE ACTIVATE(16): CMR to SMR (zone addressing)" $*

# Get drive information
zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "No sequential zones are supported by the device"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a CMR domain that is convertible to SMR
zbc_test_search_domain_by_type_and_cvt "0x1|0x4" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently CMR and convertible to SMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z ${device} ${domain_conv_start} ${domain_conv_len} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "ACTIVATE failed to smr_type=${smr_type}"

if [ -z "${sk}" ]; then
    # Verify that the domain is converted
    zbc_test_get_cvt_domain_info
    zbc_test_search_cvt_domain_by_number ${domain_num}
    if [[ $? -ne 0 || ${domain_type} != @(0x2|0x3) ]]; then
        sk=${domain_type}
        expected_sk="0x2|0x3"
    fi
fi

# Check failed
zbc_test_check_no_sk_ascq
zbc_test_check_failed

