#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2023, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

EXTENDED_TEST="Y"

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: deactivate written conventional zone OK" $*

zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

# Find a CONV realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_CONV}" "seq" "NOFAULTY"
conv_lba=$(zbc_realm_cmr_start)
seq_lba=$(zbc_realm_smr_start)
conv_len=$(zbc_realm_cmr_len)
seq_len=$(zbc_realm_smr_len)
initial_realm_type=${realm_type}
dest_type=$(type_of_type_name ${smr_type})

zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone \
			${device} ${seq_lba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z \
			${device} ${conv_lba} ${conv_len} ${cmr_type}

# Start testing
# Write some data to the first zone of the realm
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
				${conv_lba} ${lblk_per_pblk}

zbc_test_fail_exit_if_sk_ascq \
    "Initial write lba=${conv_lba} zone_type=${initial_realm_type}"

# Attempt to activate the realm as SWR or SWP
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${seq_lba} ${seq_len} ${smr_type}

zbc_test_fail_exit_if_sk_ascq "${initial_realm_type} to ${dest_type}"

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
    # ACTIVATE command should change realm to sequential type
    zbc_test_search_zone_realm_by_number ${realm_num}
    if [[ $? -ne 0 || ${realm_type} != ${dest_type} ]]; then
	zbc_test_fail_exit "ACTIVATE unchanged realm ${realm_num} type ${realm_type}-X->${dest_type}"
    fi
fi

zbc_test_check_no_sk_ascq
