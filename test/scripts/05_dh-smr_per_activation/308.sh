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

zbc_test_init $0 "ZONE ACTIVATE(32): LBA range crosses domain boundary (zone addr)" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get information
zbc_test_get_device_info

zbc_test_get_zone_info
zbc_test_get_zone_realm_info

zbc_test_count_zone_realms		# into nr_realms
zbc_test_search_zone_realm_by_number $(( ${nr_realms} - 1 ))

target_lba=$(zbc_realm_cmr_start)
realm_cmr_len=$(zbc_realm_cmr_len)
realm_smr_len=$(zbc_realm_smr_len)
target_nzone=$(( ${zbc_realm_cmr_len:-0} + ${zbc_realm_smr_len:-0} ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${target_lba} ${target_nzone} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Post process
zbc_test_check_failed
rm -f ${zone_info_file}
