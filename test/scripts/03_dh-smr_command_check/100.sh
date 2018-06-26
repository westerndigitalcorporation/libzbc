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

zbc_test_init $0 "ZONE ACTIVATE(32): in excess of max_activate (realm addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x0400"	# MAXDX

# Get drive information
zbc_test_get_device_info

if [ ${maxact_control} -eq 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting MAXIMUM ACTIVATION"
fi

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "No sequential zones are supported by the device"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find a conventional realm that can be activated as sequential
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently conventional and can be activated as sequential"
fi

# Assume that all the realms that can be activated are contiguous
zbc_test_count_actv_as_seq_realms

# Set the maximum realms that can be activated too small for the number of zones
maxd=$(( ${nr_actv_as_seq_realms} - 1 ))

# Lower the maximum number of realms to activate
zbc_test_run ${bin_path}/zbc_test_dev_control -q -maxd ${maxd} ${device}

# Make sure the command succeeded
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Failed to change max_activate to ${maxd}"

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 ${device} ${realm_num} ${nr_actv_as_seq_realms} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err "ACTIVATE type=${smr_type} realm=${realm_num} count=${nr_actv_as_seq_realms}"

# Check failed
zbc_test_check_failed

# Post-process
zbc_test_run ${bin_path}/zbc_test_dev_control -q -maxd unlimited ${device}
