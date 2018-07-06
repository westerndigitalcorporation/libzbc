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

zbc_test_init $0 "ZONE ACTIVATE(32): deactivate Explicitly-OPEN but unwritten SMR zone" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4100"	# CBI | ZNRESET

zbc_test_get_device_info

# Get zone realm information
zbc_test_get_zone_realm_info

# Find a CMR realm that can be activated as SMR
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq" "NOFAULTY"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and can be activated as SMR"
fi
expected_err_cbf="$(zbc_realm_smr_start)"

# Start testing
if [ cmr_type = "sobr" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1
fi

# Activate the realm as SMR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_cmr_start) $(zbc_realm_cmr_len) ${smr_type}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Failed to activate realm as SMR type ${smr_type}"

# Open the first zone of the realm, but do not write any data into it
zbc_test_run ${bin_path}/zbc_test_open_zone ${device} $(zbc_realm_smr_start)
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "OPEN of zone failed at $(zbc_realm_smr_start) zone_type=${smr_type}"

# Now try to activate the realm back as CMR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_smr_start) $(zbc_realm_smr_len) ${cmr_type}

# Check result
zbc_test_get_sk_ascq
if [ "${smr_type}" = "seqp" ]; then
    #XXX Arguably a SEQP zone must be empty to deactivate, but the emulator allows non-empty for now
    zbc_test_check_no_sk_ascq "${smr_type} to ${cmr_type}"
else
    zbc_test_check_err "activate ${smr_type} as ${cmr_type}"
fi

# Post-processing -- put the realm back the way we found it
zbc_test_check_failed
if [ "${smr_type}" != "seqp" ]; then
    # Zone did not deactivate -- reset and deactivate it
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} $(zbc_realm_smr_start)
    zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(zbc_realm_smr_start) $(zbc_realm_smr_len) ${cmr_type}
fi
