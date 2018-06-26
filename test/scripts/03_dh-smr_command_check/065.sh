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

zbc_test_init $0 "ZONE ACTIVATE(32): non-activation SMR to CMR (zone addressing)" $*

# Set expected error code
expected_sk="Aborted-command"
expected_asc="Activation-type-unsupported"

# Get drive information
zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${sobr_zone} -ne 0 ]; then
    cmr_type="sobr"
else
    zbc_test_print_not_applicable "No non-sequential zones are supported by the device"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the first SMR realm that cannot be activated as CMR
zbc_test_search_realm_by_type_and_actv "0x2|0x3" "noconv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently SMR and can't be activated sa CMR"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z -32 ${device} ${realm_seq_start} ${realm_seq_len} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Check failed
zbc_test_check_failed

