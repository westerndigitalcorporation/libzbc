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

zbc_test_init $0 "ZONE ACTIVATE(16): all SMR to CMR (zone addressing)" $*

# Get drive information
zbc_test_get_device_info

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the first SMR realm that can be activated as CMR
zbc_test_search_realm_by_type_and_actv "${ZT_SEQ}" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently SMR and can be activated as CMR"
fi

# Assume that all the realms that can be activated are contiguous
start_cmr_lba=$(zbc_realm_cmr_start)

# Check whether our attempted activation will exceed the limit
zbc_test_calc_nr_realm_zones ${realm_num} ${nr_actv_as_conv_realms}
max_act=`zbc_info ${device} | grep "Maximum number of zones to activate" | sed -e "s/.* //"`  #XXX
if [[ ${max_act} != "unlimited" && ${max_act} -lt ${nr_conv_zones} ]]; then
    expected_sk="${ERR_ZA_SK}"
    expected_asc="${ERR_ZA_ASC}"
    expected_err_za="0x0400"        # MAXRX - attempt to exceed maximum conversion
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z ${device} ${start_cmr_lba} ${nr_conv_zones} ${cmr_type}

# Check result
zbc_test_get_sk_ascq

if [ -z "${sk}" ]; then
    # Verify that no SMR realms that can be activated as CMR are present
    zbc_test_get_zone_realm_info
    zbc_test_search_realm_by_type_and_actv "${ZT_SEQ}" "conv"
    if [ $? -eq 0 ]; then
	sk=${realm_num}
        expected_sk="no-seq-to-conv"
    fi
fi

if [ -z "${expected_sk}" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Check failed
zbc_test_check_failed
