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

zbc_test_init $0 "ZONE ACTIVATE(32): attempt to deactivate Explicitly-OPEN but unwritten sequential zone (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4100"	# CBI | ZNRESET

zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "No sequential zones are supported by the device"
fi

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${wpc_zone} -ne 0 ]; then
    cmr_type="wpc"
else
    zbc_test_print_not_applicable "No non-sequential zones are supported by the device"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a conventional domain that is convertible to sequential
zbc_test_search_domain_by_type_and_cvt "${ZT_NON_SEQ}" "seq" "NOFAULTY"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently conventional and convertible to sequential"
fi
expected_err_cbf="${domain_seq_start}"

# Start testing
if [ cmr_type = "wpc" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1
fi

# Convert the domain to sequential
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_conv_start} ${domain_conv_len} ${smr_type}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Failed to convert domain to sequential type ${smr_type}"

# Open the first zone of the domain, but do not write any data into it
zbc_test_run ${bin_path}/zbc_test_open_zone ${device} ${domain_seq_start}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "OPEN of zone failed at ${domain_seq_start} zone_type=${smr_type}"

# Now try to convert the domain from sequential back to conventional
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_seq_start} ${domain_seq_len} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
if [ "${smr_type}" = "seqp" ]; then
    #XXX Arguably a SEQP zone must be empty to deactivate, but the emulator allows non-empty for now
    zbc_test_check_no_sk_ascq "${smr_type} to ${cmr_type}"
else
    zbc_test_check_err "convert ${smr_type} to ${cmr_type}"
fi

# Post-processing -- put the domain back the way we found it
zbc_test_check_failed
if [ "${smr_type}" != "seqp" ]; then
    # Zone did not deactivate -- reset and deactivate it
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${domain_seq_start}
    zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_seq_start} ${domain_seq_len} ${cmr_type}
fi
