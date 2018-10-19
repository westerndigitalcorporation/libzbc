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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: deactivate implicitly-open SOBR zone" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="${ZA_STAT_NOT_EMPTY}"

zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

# Find a SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_SOBR}" "seq" "NOFAULTY"
expected_err_cbf=$(zbc_realm_cmr_start)

# Start testing
# Make sure the deactivating zones are EMPTY
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1

# Make the first zone of the realm non-empty
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			$(zbc_realm_cmr_start) ${lblk_per_pblk}
zbc_test_fail_exit_if_sk_ascq "Initial write lba=$(zbc_realm_cmr_start) zone_type=SOBR"

# Attempt to activate the realm as SWR or SWP
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} $(zbc_realm_smr_start) $(zbc_realm_smr_len) ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err "ACTIVATE SOBR as ${smr_type}"
