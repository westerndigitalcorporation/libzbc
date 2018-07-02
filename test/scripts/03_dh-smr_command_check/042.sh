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

zbc_test_init $0 "ZONE ACTIVATE(16): CMR to SMR (zone addressing, FSNOZ)" $*

# Get drive information
zbc_test_get_device_info

if [ ${za_control} == 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting FSNOZ"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find a CMR realm that can be actviated as SMR
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and can be activated as SMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z -n ${device} $(zbc_realm_smr_start) $(zbc_realm_smr_len) ${smr_type}

# Check result
zbc_test_get_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that the realm has been activated
    zbc_test_get_zone_realm_info
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != @(${ZT_SEQ}) ]]; then
        sk=${realm_type}
        expected_sk="${ZT_SEQ}"
    fi
fi

# Check failed
zbc_test_check_no_sk_ascq
zbc_test_check_failed

