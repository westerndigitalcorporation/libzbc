#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2023, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

EXTENDED_TEST="Y"

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: all type ${test_deactv_type:-${ZT_SEQ}} to CONV or SOBR (realm addressing)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

if [[ "${test_deactv_type}" == "${ZT_SWP}" ]]; then
    if [ ${seq_pref_zone} -eq 0 ]; then
	zbc_test_print_not_applicable "Device does not support deactivation of SWP zone type"
    fi
fi
seq_type="${test_deactv_type:-${ZT_SEQ}}"

# Assume that all the realms that can be activated are contiguous
# Find the first SWR or SWP realm that can be activated as CONV or SOBR
zbc_test_search_realm_by_type_and_actv_or_NA "${seq_type}" "conv" "NOFAULTY"
initial_realm_type=${realm_type}

actv_realms=${nr_actv_as_seq_realms}
if [ $(expr "${realm_num}" + "${actv_realms}") -gt ${nr_realms} ]; then
    actv_realms=$(expr "${nr_realms}" - "${realm_num}")
fi

zbc_test_search_seq_zone_cond "${ZC_OFFLINE}|${ZC_RDONLY}"
if [ $? -eq 0 ]; then
    # Device has a faulty zone -- make sure it's not in the way
    if [[ ${target_slba} -ge $(zbc_realm_cmr_start) ]]; then
	# Unlikely: the faulty zone is above the realm we found above
	zbc_test_print_not_applicable "Faulty zones interfere with this test"
    fi
fi

# Check whether our attempted activation will exceed the limit
zbc_test_calc_nr_realm_zones ${realm_num} ${actv_realms}

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} \
			${device} ${realm_num} ${actv_realms} ${cmr_type}

if [[ ${max_act} != "unlimited" && ${max_act} -lt ${nr_seq_zones} ]]; then
    expected_sk="Illegal-request"
    expected_asc="Invalid-field-in-cdb"
    zbc_test_check_sk_ascq "realm=${realm_num} type=${initial_realm_type}->${seq_type}"
    exit
fi

zbc_test_fail_exit_if_sk_ascq "realm=${realm_num} type=${initial_realm_type}->${seq_type}"

# ACTIVATE or QUERY succeeded -- update realm info and check realm type
zbc_test_get_zone_realm_info

is_substr "--query" "${zbc_test_actv_flags}"
if [ $? -eq 0 ]; then
    # QUERY command should leave realms unchanged
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != ${initial_realm_type} ]]; then
	zbc_test_fail_exit "QUERY changed realm type ${initial_realm_type}->${realm_type}"
    fi
else
    # ACTIVATE command should have changed type of all realms
    zbc_test_search_realm_by_type_and_actv "${initial_realm_type}" "conv" "NOFAULTY"
    if [[ $? -eq 0 ]]; then
	zbc_test_fail_exit \
	    "ACTIVATE unchanged realm ${realm_num} type ${realm_type}-X->${dest_type}"
    fi
fi

zbc_test_check_no_sk_ascq
