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

zbc_test_init $0 "ZONE ACTIVATE(16): all CMR to SMR (zone addressing, FSNOZ)" $*

# Get drive information
zbc_test_get_device_info

if [ ${za_control} == 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting FSNOZ"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find a CMR realm that can be activated as SMR
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and can be activated as SMR"
fi

# Assume that all the realms that can be activated are contiguous
start_smr_lba=$(zbc_realm_smr_start)

# Calculate the total number of zones in this range of realms
zbc_test_calc_nr_realm_zones ${realm_num} ${nr_actv_as_seq_realms}

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z -n ${device} ${start_smr_lba} ${nr_seq_zones} ${smr_type}

# Check result
zbc_test_get_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that no CMR realms that can be activated as SMR are present
    zbc_test_get_zone_realm_info
    zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq"
    if [ $? -eq 0 ]; then
	sk=${realm_num}
	expected_sk="no-conv-to-seq"
    fi
fi


# Check failed
zbc_test_check_no_sk_ascq
zbc_test_check_failed

