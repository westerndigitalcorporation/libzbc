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

zbc_test_init $0 "ZONE ACTIVATE(32): deactivate Implicitly-OPEN written non-sequential zone" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4100"	# CBI | ZNRESET

zbc_test_get_device_info

# Get realm information
zbc_test_get_zone_realm_info

# Find a non-sequential realm that can be activated as sequential
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq" "NOFAULTY"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently non-sequential and can be activated as sequential"
fi
expected_err_cbf="$(zbc_realm_cmr_start)"

# Start testing
if [ cmr_type = "sobr" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1
fi

# Make the first zone of the realm non-empty
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} $(zbc_realm_cmr_start) ${lblk_per_pblk}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed at $(zbc_realm_cmr_start) zone_type=${cmr_type}"

# Attempt to activate the realm as sequential
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_cmr_start) $(zbc_realm_cmr_len) ${smr_type}

# Check result
zbc_test_get_sk_ascq
if [ "${cmr_type}" = "conv" ]; then
    zbc_test_check_no_sk_ascq "${cmr_type} to ${smr_type}"	# conventional zone does not expect failure
else
    zbc_test_check_err "activate ${cmr_type} as ${smr_type}"	# SOBR zone expects failure
fi

# Post-processing -- put the elmback the way we found it
zbc_test_check_failed
if [ "${cmr_type}" == "conv" ]; then
    # Convert the realm back to its starting type
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} $(zbc_realm_smr_start)
    zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_smr_start) $(zbc_realm_smr_len) ${cmr_type}
fi
