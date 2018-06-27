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

zbc_test_init $0 "ZONE ACTIVATE(16): non-activation CMR to SMR (zone addr, FSNOZ)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x0080"

# Get drive information
zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "No sequential zones are supported by the device"
fi

if [ ${za_control} == 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting FSNOZ"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the first CMR realm that cannot be activated as SMR
zbc_test_search_realm_by_type_and_actv "0x1|0x4" "noseq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and can't be activated as SMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z -n ${device} ${realm_conv_start} ${realm_conv_len} ${smr_type}
if [ $? -eq 2 ]; then
   zbc_test_print_passed
   exit 0
fi

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

