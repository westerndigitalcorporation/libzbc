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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: non-activation type ${test_deactv_type:-${ZT_SEQ}} to unsupported CONV or SOBR (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="${ZA_STAT_UNSUPP}"

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

# Find the first SWR or SWP realm that cannot be activated as CONV or SOBR
zbc_test_search_realm_by_type_and_actv_or_NA "${seq_type}" "noconv"

lba=$(zbc_realm_cmr_start)
if [ $? -ne 0 ]; then
   zbc_test_print_passed_lib "No realm_cmr_start"
   exit
fi

len=$(zbc_realm_cmr_len)
if [ $? -ne 0 ]; then
   zbc_test_print_passed_lib "No realm_cmr_len"
   exit
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${lba} ${len} ${cmr_type}
if [ $? -eq 2 ]; then
   zbc_test_print_passed_lib "zbc_test_zone_activate"
   exit
fi

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err
