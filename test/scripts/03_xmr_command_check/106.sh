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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: NZONE not realm-integral (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="${ZA_STAT_REALM_ALIGN}"

zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

# Find a CONV or SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq"

if [ $(zbc_realm_smr_len) -lt 2 ]; then
    zbc_test_print_not_applicable "Sequential realms have fewer than two zones"
fi

# Subtract one from the number of zones to realm-misalign it for the test
len=$(( $(zbc_realm_smr_len) - 1 ))
expected_err_cbf=$(zbc_realm_smr_start)

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} $(zbc_realm_smr_start) ${len} ${smr_type}

# Check result
if [ ${zone_realms_device} -ne 0 ]; then
    zbc_test_get_sk_ascq
    zbc_test_check_err
else
    zbc_test_check_no_sk_ascq
fi
