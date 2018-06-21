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

zbc_test_init $0 "ZONE ACTIVATE(16): SWR to CMR (domain addressing, FSNOZ)" $*

# Get drive information
zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${wpc_zone} -ne 0 ]; then
    cmr_type="wpc"
else
    zbc_test_print_not_applicable "No non-sequential zones are supported by the device"
fi

if [ ${za_control} == 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting FSNOZ"
fi

# Get domain information
zbc_test_get_cvt_domain_info

# Find a SWR domain that is convertible to CMR
zbc_test_search_domain_by_type_and_cvt "0x2|0x3" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently SMR and convertible to CMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -n ${device} ${domain_num} 1 ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that the domain is converted
    zbc_test_get_cvt_domain_info
    zbc_test_search_cvt_domain_by_number ${domain_num}
    if [[ $? -ne 0 || ${domain_type} != @(0x1|0x4) ]]; then
        sk=${domain_type}
        expected_sk="0x1|0x4"
        zbc_test_print_failed_sk
    fi
fi

# Check failed
zbc_test_check_failed

