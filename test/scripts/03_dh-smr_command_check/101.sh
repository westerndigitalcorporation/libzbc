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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: LBA starting out of range (zone addressing)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

# Select the realm containing the last zone
zbc_test_search_realm_by_lba ${last_zone_lba}

cmr_len=$(zbc_realm_cmr_len)
smr_len=$(zbc_realm_smr_len)

# Use the size of the last realm when trying ACTIVATE at End of Medium
if [ ${smr_len:-0} -gt 0 ]; then
    nzone=${smr_len}
elif [ ${cmr_len:-0} -gt 0 ]; then
    nzone=${cmr_len}
else
    zbc_test_fail_exit \
	"WARNING: Realm ${realm_num} has cmr and smr length both zero"
fi

expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"	# ZONE ID + NUMBER OF ZONES out of range

# Start testing
# Try to activate starting at the maximum device LBA
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} $(( ${max_lba} + 1 )) ${nzone} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
