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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: type ${test_deactv_type:-${ZT_SEQ}} to CONV or SOBR (zone addressing)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

zbc_test_realm_boundaries_not_shifting_or_NA "${cmr_type}"

if [[ "${test_deactv_type}" == "${ZT_SWP}" ]]; then
    if [ ${seq_pref_zone} -eq 0 ]; then
	zbc_test_print_not_applicable "Device does not support deactivation of SWP zone type"
    fi
fi

seq_type="${test_deactv_type:-${ZT_SEQ}}"

# Find a SWR or SWP realm that can be activated as CONV or SOBR
zbc_test_search_realm_by_type_and_actv_or_NA "${seq_type}" "conv" "NOFAULTY"
initial_realm_type=${realm_type}
realm_number=${realm_num}

# We want to activate two ajacent realms. Find the number of zones
# in the the next realm - it might be different from the first
next_realm=$(( ${realm_number} + 1 ))
zbc_test_search_zone_realm_by_number ${next_realm}
if [[ "${realm_num}" != "${next_realm}" ]]; then
	second_realm_cmr_len=0
else
	second_realm_cmr_len=$(zbc_realm_cmr_len)
fi
zbc_test_search_zone_realm_by_number ${realm_number}

if [[ ${cmr_type} == "sobr" ]]; then
    dest_type=${ZT_SOBR}
else
    dest_type=${ZT_CONV}
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
		         ${device} $(zbc_realm_cmr_start) \
                         $(( $(zbc_realm_cmr_len) + ${second_realm_cmr_len} )) ${cmr_type}

# Check result
zbc_test_fail_exit_if_sk_ascq

# Update realm info
zbc_test_get_zone_realm_info

is_substr "--query" "${zbc_test_actv_flags}"
if [ $? -eq 0 ]; then
    # QUERY command should leave realm unchanged
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != ${initial_realm_type} ]]; then
	zbc_test_fail_exit "QUERY changed realm type ${initial_realm_type}->${realm_type}"
    fi
else
    # ACTIVATE command should change realm to active
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != ${dest_type} ]]; then
	zbc_test_fail_exit "ACTIVATE unchanged realm ${realm_num} type ${realm_type}-X->${dest_type}"
    fi
fi

zbc_test_check_no_sk_ascq
