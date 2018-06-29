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

zbc_test_init $0 "ZONE ACTIVATE(32): attempt to deactivate Implicitly-OPEN partly-written non-sequential zone (zone addressing)" $*

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
expected_err_cbf="${domain_conv_start}"

# Start testing
if [ cmr_type = "wpc" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1
fi

# Make the first zone of the domain non-empty
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} ${domain_conv_start} ${lblk_per_pblk}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed at ${domain_conv_start} zone_type=${cmr_type}"

# Attempt to convert the domain to sequential
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_conv_start} ${domain_conv_len} ${smr_type}

# Check result
zbc_test_get_sk_ascq
if [ "${cmr_type}" = "conv" ]; then
    zbc_test_check_no_sk_ascq "${cmr_type} to ${smr_type}"	# conventional zone does not expect failure
else
    zbc_test_check_err "convert ${cmr_type} to ${smr_type}"	# WPC zone expects failure
fi

# Post-processing -- put the domain back the way we found it
zbc_test_check_failed
if [ "${cmr_type}" == "conv" ]; then
    # Convert the realm back to its starting type
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${domain_seq_start}
    zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_seq_start} ${domain_seq_len} ${cmr_type}
fi