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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: LBA range crosses domain boundary (zone addr)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

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
elif [[ ${cmr_len:-0} -eq 0 ]]; then
    target_lba=$(zbc_realm_smr_start)
    nzone=$(( 2 * ${smr_len} ))
else
    target_lba=$(zbc_realm_cmr_start)
    nzone=$(( ${cmr_len} + ${smr_len} ))
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
    expected_err_cbf=${target_lba}
fi

# Start testing
# Try to activate a zone range spanning the realm of the last CONV or SOBR zone
# and the realm of the the first SWR or SWP zone.
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${target_lba} ${nzone} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err