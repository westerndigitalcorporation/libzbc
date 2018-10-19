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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: CONV or SOBR to type ${test_actv_type:-"sequential"} (zone addressing)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

if [[ "${test_actv_type}" == "seqp" ]]; then
    if [ ${seq_pref_zone} -eq 0 ]; then
	zbc_test_print_not_applicable "Device does not support activation of SWP zone type"
    fi
fi

seq_type_name=${test_actv_type:-${smr_type}}
seq_type=$(type_of_type_name ${seq_type_name})

# Find a CONV or SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq" "NOFAULTY"
initial_realm_type=${realm_type}

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} $(zbc_realm_start ${seq_type_name}) \
			$(( 2 * $(zbc_realm_smr_len) )) ${seq_type_name}

# Check result
zbc_test_fail_exit_if_sk_ascq

# Update realm info
zbc_test_get_zone_realm_info

is_substr "--query" "${zbc_test_actv_flags}"
if [ $? -eq 0 ]; then
    # QUERY command should leave realm unchanged
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != ${initial_realm_type} ]]; then
	zbc_test_fail_exit "QUERY changed realm type ${initial_realm_type}->${realm_type}"
    fi
else
    # ACTIVATE command should change realm to active
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != ${seq_type} ]]; then
	zbc_test_fail_exit "ACTIVATE unchanged realm ${realm_num} type ${realm_type}-X->${seq_type}"
    fi
fi

zbc_test_check_no_sk_ascq
