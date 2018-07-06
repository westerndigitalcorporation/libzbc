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

zbc_test_init $0 "ZONE ACTIVATE(32): deactivate FULL Sequential-or-Before-Required zone" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4100"	# CBI | ZNRESET

zbc_test_get_device_info

# Get realm information
zbc_test_get_zone_realm_info

# Find a SOBR realm that can be activated as sequential
zbc_test_search_realm_by_type_and_actv "${ZT_SOBR}" "seq" "NOFAULTY"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently Sequential-or-Before-Required and can be activated as SMR"
fi
expected_err_cbf="$(zbc_realm_cmr_start)"

zbc_test_get_zone_info
zbc_test_get_target_zone_from_slba $(zbc_realm_cmr_start)

# Start testing
# Make sure the deactivating zones are EMPTY
zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1

# Make the first zone of the realm FULL
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} $(zbc_realm_cmr_start) ${target_size}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed at $(zbc_realm_cmr_start) zone_type=${cmr_type}"

# Attempt to activate the realm as sequential
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_cmr_start) $(zbc_realm_cmr_len) ${smr_type}

# Check result
zbc_test_get_sk_ascq
if [ "${cmr_type}" = "conv" ]; then
    zbc_test_check_no_sk_ascq "${cmr_type} to ${smr_type}"	# conventional zone does not expect failure
else
    zbc_test_check_err "activate ${cmr_type} sa ${smr_type}"	# SOBR zone expects failure
fi

# Post-processing -- put the realm back the way we found it
zbc_test_check_failed
if [ "${cmr_type}" == "conv" ]; then
    # Activate the realm back as its starting type
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} $(zbc_realm_smr_start)
    zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_smr_start) $(zbc_realm_smr_len) ${cmr_type}
fi
