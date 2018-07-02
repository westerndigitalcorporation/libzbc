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

zbc_test_init $0 "ZONE ACTIVATE(32): SWR to CMR (realm addressing)" $*

# Get drive information
zbc_test_get_device_info

# Get zone realm information
zbc_test_get_zone_realm_info

# Find a SWR realm that can be activated as CMR
zbc_test_search_realm_by_type_and_actv "0x2|0x3" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently SMR and can be activated as CMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 ${device} ${realm_num} 1 ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that the realm has been activated
    zbc_test_get_zone_realm_info
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != @(${ZT_NON_SEQ}) ]]; then
        sk=${realm_type}
        expected_sk="${ZT_NON_SEQ}"
        zbc_test_print_failed_sk
    fi
fi

# Check failed
zbc_test_check_failed

