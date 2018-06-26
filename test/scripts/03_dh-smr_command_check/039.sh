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

zbc_test_init $0 "ZONE ACTIVATE(16): all SMR to CMR (realm addressing, FSNOZ)" $*

# Get drive information
zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${sobr_zone} -ne 0 ]; then
    cmr_type="sobr"
else
    zbc_test_print_not_applicable "No non-sequential zones are supported by the device"
fi

if [ ${za_control} == 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting FSNOZ"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the first SMR realm that can be activated as CMR
zbc_test_search_realm_by_type_and_actv "0x2|0x3" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently SMR and can be activated as CMR"
fi

# Assume that all the realms that can be activated are contiguous
zbc_test_count_zone_realms
zbc_test_count_actv_as_conv_realms
if [ $(expr "${realm_num}" + "${nr_actv_as_conv_realms}") -gt ${nr_realms} ]; then
    nr_actv_as_conv_realms=$(expr "${nr_realms}" - "${realm_num}")
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -n ${device} ${realm_num} ${nr_actv_as_conv_realms} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "ACTIVATE failed to cmr_type=${cmr_type}"

if [ -z "${sk}" ]; then
    # Verify that no SMR realms that can be activated as CMR are present
    zbc_test_get_zone_realm_info
    zbc_test_search_realm_by_type_and_actv "0x2|0x3" "conv"
    if [ $? -eq 0 ]; then
        sk=${realm_num}
        expected_sk="no-seq-to-conv"
    fi
fi

# Check failed
zbc_test_check_no_sk_ascq
zbc_test_check_failed

