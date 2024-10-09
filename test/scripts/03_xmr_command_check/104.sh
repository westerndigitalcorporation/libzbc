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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: LBA not zone-aligned (zone addressing)" $*

expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"	# ZONE ID not the lowest LBA of a Zone

zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"
zbc_test_cdb32_check_or_NA "${zbc_test_actv_flags}"
zbc_test_nozsrc_check_or_NA "${zbc_test_actv_flags}"

if [ ${lblk_per_zone} -lt 2 ]; then
    zbc_test_print_not_applicable "Device has fewer than two logical blocks per zone"
fi

# Find a CONV or SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq"

# Add one to the starting LBA to zone-misalign it for the test
start_lba=$(( $(zbc_realm_smr_start) + 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${start_lba} $(zbc_realm_smr_len) ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
