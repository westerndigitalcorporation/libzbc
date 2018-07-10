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
zbc_test_get_zone_realm_info

zbc_test_search_zone_realm_by_number $(( ${nr_realms} - 1 ))

cmr_start=$(zbc_realm_cmr_start)
smr_start=$(zbc_realm_smr_start)
cmr_len=$(zbc_realm_cmr_len)
smr_len=$(zbc_realm_smr_len)

msg="WARNING: Realm $(( ${nr_realms} - 1 )) has neither cmr_start nor smr_start"
target_lba=${cmr_start:-${smr_start:?"${msg}"}}

# Use the sum of the realm sizes in CMR and SMR domains (if both available)
target_nzone=$(( ${cmr_len:-${smr_len:-0}} + ${smr_len:-${cmr_len:-0}} ))

# Start testing
# Try to activate a zone range spanning the realm of the last CMR zone and realm of the the first SMR zone.
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${target_lba} ${target_nzone} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Post process
zbc_test_check_failed
