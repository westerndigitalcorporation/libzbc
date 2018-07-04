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

zbc_test_init $0 "ZONE ACTIVATE(32): LBA not realm-aligned (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4001"	# CBI | Zone Boundary Violation

zbc_test_get_device_info
zbc_test_get_zone_info
zbc_test_get_zone_realm_info

# Find a non-sequential realm that can be activated as sequential
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and can be activated as SMR"
fi

# Add one zone-size to the starting zone to realm-misalign it for the test
zbc_test_get_target_zone_from_slba $(zbc_realm_smr_start)

start_lba=$(( $(zbc_realm_smr_start) + ${target_size} ))
expected_err_cbf="${start_lba}"

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${start_lba} $(zbc_realm_smr_len) ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

# Post process
rm -f ${zone_info_file}
