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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: type ${test_deactv_type:-${ZT_SEQ}} to CONV or SOBR (realm addressing)" $*

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
if [[ ${cmr_type} == "sobr" ]]; then
    dest_type=${ZT_SOBR}
else
    dest_type=${ZT_CONV}
fi

# Find a SWR or SWP realm that can be activated as CONV or SOBR
zbc_test_search_realm_by_type_and_actv_or_NA "${seq_type}" "conv" "NOFAULTY"
initial_realm_type=${realm_type}

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} \
			${device} ${realm_num} 2 ${cmr_type}

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
