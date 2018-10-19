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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: LBA not realm-aligned (zone addressing)" $*

zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

# Find a CONV or SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq"

nzone=$(zbc_realm_smr_len)
if [ ${nzone} -lt 2 ]; then
    zbc_test_print_not_applicable "Sequential realms have fewer than two zones"
fi

zbc_test_get_target_zone_from_slba $(zbc_realm_smr_start)

# Add one zone-size to the starting zone to realm-misalign it for the test
target_lba=$(( $(zbc_realm_smr_start) + ${target_size} ))

if [ $(( ${target_lba} + ${nzone} * ${lblk_per_zone} )) -gt $(( ${max_lba} + 1 )) ]; then
    # ZONE ID plus NUMBER OF ZONES is out of range
    expected_sk="Illegal-request"
    expected_asc="Invalid-field-in-cdb"
else
    expected_sk="${ERR_ZA_SK}"
    expected_asc="${ERR_ZA_ASC}"
    expected_err_za="${ZA_STAT_REALM_ALIGN}"	# ZONE ID not the lowest LBA of a Realm
    expected_err_cbf=${target_lba}
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${target_lba} ${nzone} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err
