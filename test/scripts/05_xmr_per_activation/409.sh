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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: LBA range crosses domain boundary (zone addr)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

# Select last realm
zbc_test_search_zone_realm_by_number $(( ${nr_realms} - 1 ))

cmr_len=$(zbc_realm_cmr_len)
smr_len=$(zbc_realm_smr_len)

# Use the sum of the realm sizes in CONV or SOBR and SWR or SWP
# domains when attempting to cross their boundary
# (if both are available, otherwise use double whichever one we have)
if [[ ${smr_len:-0} -eq 0 ]]; then
    target_lba=$(zbc_realm_cmr_start)
    nzone=$(( 2 * ${cmr_len:-0} ))
    fail_lba=$(( ${target_lba} + ${cmr_len:-0} * ${lblk_per_zone} ))
elif [[ ${cmr_len:-0} -eq 0 ]]; then
    target_lba=$(zbc_realm_smr_start)
    nzone=$(( 2 * ${smr_len} ))
    fail_lba=$(( ${target_lba} + ${smr_len} * ${lblk_per_zone} ))
else
    target_lba=$(zbc_realm_cmr_start)
    nzone=$(( ${cmr_len} + ${smr_len} ))
    fail_lba=$(( ${target_lba} + ${cmr_len} * ${lblk_per_zone} ))
fi

if [[ ${nzone} -eq 0 ]]; then
    zbc_test_fail_exit "WARNING: Realm ${realm_num} has cmr and smr length both zero"
fi

if [ $(( ${target_lba} + ${nzone} * ${lblk_per_zone} )) -gt $(( ${max_lba} + 1 )) ]; then
    # ZONE ID plus NUMBER OF ZONES is out of range
    expected_sk="Illegal-request"
    expected_asc="Invalid-field-in-cdb"
else
    expected_sk="${ERR_ZA_SK}"
    expected_asc="${ERR_ZA_ASC}"
    expected_err_za="${ZA_STAT_CROSS_DOMAINS}"	# Crossing domain boundary
    expected_err_cbf=${fail_lba}
fi

# Start testing
# Try to activate a zone range spanning the realm of the last CONV or SOBR zone
# and the realm of the the first SWR or SWP zone.
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${target_lba} ${nzone} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err
