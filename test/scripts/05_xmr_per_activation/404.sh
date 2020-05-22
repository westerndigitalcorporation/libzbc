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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: deactivate FULL SWR or SWP zone" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="${ZA_STAT_NOT_EMPTY}"

zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

# Find a CONV or SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq" "NOFAULTY"
conv_lba=$(zbc_realm_cmr_start)
seq_lba=$(zbc_realm_smr_start)
conv_len=$(zbc_realm_cmr_len)
seq_len=$(zbc_realm_smr_len)
expected_err_cbf=${seq_lba}

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone \
			${device} ${seq_lba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z \
			${device} ${conv_lba} ${conv_len} ${cmr_type}

# Start testing
if [ ${cmr_type} = "sobr" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
fi

# Activate the realm as SWR or SWP
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z \
			${device} ${seq_lba} ${seq_len} ${smr_type}
zbc_test_fail_exit_if_sk_ascq "Initial ACTIVATE realm as type ${smr_type}"

zbc_test_get_target_zone_from_slba ${seq_lba}

# Make the first zone of the realm FULL
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			${seq_lba} ${target_size}
zbc_test_fail_exit_if_sk_ascq "Initial WRITE lba=${seq_lba} zone_type=${smr_type}"

# Now try to activate the realm back as CONV or SOBR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${conv_lba} ${conv_len} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err "ACTIVATE ${smr_type} as ${cmr_type}"
