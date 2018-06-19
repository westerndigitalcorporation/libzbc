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

zbc_test_init $0 "ZONE ACTIVATE(16): non-convertible CMR to SMR (realm addressing)" $*

# Get drive information
zbc_test_get_device_info

if [ ${seq_pref_zone} -eq 0 ]; then
    zbc_test_print_not_applicable "Device does not support conversion to SWP zone type"
fi

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x0080"

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the first CMR realm that is not convertible to SMR
zbc_test_search_realm_by_type_and_cvt "0x1|0x4" "noseq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and NON-convertible to SMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${realm_conv_start} ${realm_conv_len} "seqp"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

