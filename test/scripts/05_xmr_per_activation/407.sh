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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: deactivate Implicitly-OPEN non-first realm last zone" $*

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
# NOFAULTY also gets us two in a row
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq" "NOFAULTY"

# Record ACTIVATE start LBA for both directions
conv_lba=$(zbc_realm_cmr_start)
seq_lba=$(zbc_realm_smr_start)

# Calculate number of zones in two realms starting at ${realm_num}
zbc_test_calc_nr_realm_zones ${realm_num} 2

# Record ACTIVATE number of zones for both directions
conv_nz=${nr_conv_zones}
seq_nz=${nr_seq_zones}

# Lookup info on the second realm
zbc_test_search_zone_realm_by_number $(( ${realm_num} + 1 ))

# Lookup info on the second realm's first SWR or SWP zone
zbc_test_get_target_zone_from_slba $(zbc_realm_smr_start)

# Calculate the start LBA of the second realm's last zone
write_lba=$(( ${target_slba} + $(zbc_realm_smr_len) * ${lblk_per_zone} - ${target_size} ))
expected_err_cbf=${write_lba}

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${write_lba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z \
			${device} ${conv_lba} ${conv_nz} ${cmr_type}

# Start testing
if [ ${cmr_type} = "sobr" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
fi

# Activate the two realms as SWR or SWP
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z \
			${device} ${seq_lba} ${seq_nz} ${smr_type}
zbc_test_fail_exit_if_sk_ascq "ACTIVATE realm as type ${smr_type}"

# Write an LBA in the second zone of the second realm to make it NON-EMPTY
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${write_lba} ${lblk_per_pblk}
zbc_test_fail_exit_if_sk_ascq "Initial write lba=$(zbc_realm_smr_start) zone_type=${smr_type}"

# Now try to activate the SWR or SWP realms back as CONV or SOBR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${conv_lba} ${conv_nz} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err "ACTIVATE ${smr_type} as ${cmr_type}"
