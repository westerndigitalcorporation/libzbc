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

zbc_test_init $0 "ZONE ACTIVATE(32): deactivate Implicitly-OPEN SoBR zone with no data" $*

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

if [ ${wpc_zone} -eq 0 ]; then
    zbc_test_print_not_applicable "No Sequential-or-Before-Required zones are supported by the device"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a conventional domain that is convertible to sequential
zbc_test_search_domain_by_type_and_cvt "${ZT_SBR}" "seq" "NOFAULTY"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently Sequential-or-Before-Required and convertible to sequential"
fi
expected_err_cbf="${domain_conv_start}"

# Start testing
# Make sure the deactivating zones are EMPTY
zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1

# Implicitly open the first zone of the realm with a zero-length write
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} ${domain_conv_start} 0
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed at ${domain_conv_start} zone_type=${smr_type}"

# Attempt to convert the domain to sequential
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_conv_start} ${domain_conv_len} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Post-processing
zbc_test_check_failed
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${domain_conv_start}
