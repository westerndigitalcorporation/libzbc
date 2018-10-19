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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: in excess of max_activate (zone addressing)" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get drive information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

if [ ${maxact_control} -eq 0 ]; then
    zbc_test_print_not_applicable "Device does not support setting MAXIMUM ACTIVATION"
fi

# Find a CONV or SOBR realm that can be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq" "NOFAULTY"

zbc_test_search_seq_zone_cond_or_NA "${ZC_OFFLINE}|${ZC_RDONLY}"
if [ $? -eq 0 ]; then
    # Device has a faulty zone -- make sure it's not in the way
    if [[ ${target_slba} -ge $(zbc_realm_cmr_start) ]]; then
	# Unlikely: the faulty zone is above the realm we found above
	zbc_test_print_not_applicable "Faulty zones interfere with this test"
    fi
fi

# Assume that all the realms that can be activated are contiguous
actv_realms=${nr_actv_as_seq_realms}
if [ $(expr "${realm_num}" + "${actv_realms}") -gt ${nr_realms} ]; then
    actv_realms=$(expr "${nr_realms}" - "${realm_num}")
fi

# Set the maximum realms that can be activated too small for the number of zones
maxr=$(( ${actv_realms} - 1 ))

if [ ${maxr} -eq 0 ]; then
    zbc_test_print_not_applicable "Maximum activation cannot be set low enough"
fi

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_dev_control -q -maxr unlimited ${device}

# Start testing
# Lower the maximum number of realms to activate
zbc_test_run ${bin_path}/zbc_test_dev_control -q -maxr ${maxr} ${device}

# Make sure the command succeeded
zbc_test_fail_exit_if_sk_ascq "change max_activate to ${maxr}"

start_lba=$(zbc_realm_smr_start)
conv_lba=$(zbc_realm_cmr_start)
zbc_test_calc_nr_realm_zones ${realm_num} ${actv_realms}
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${start_lba} ${nr_seq_zones} ${smr_type}

# Check result
zbc_test_get_sk_ascq

is_substr "--fsnoz" "${zbc_test_actv_flags}"
if [ $? -eq 0 ]; then
    # We specified FSNOZ
    zbc_test_check_sk_ascq "type=${smr_type} realm=${realm_num} count=${actv_realms}"
elif [[ ${nozsrc} -eq 0 ]]; then
    # zone_activate adds its own --fsnoz when nozsrc is unsupported
    zbc_test_check_sk_ascq "type=${smr_type} realm=${realm_num} count=${actv_realms}"
else
    # XXX BOGUS SPEC: maximum activation limit does not apply when NOZSRC is set
    zbc_test_check_no_sk_ascq \
	"type=${smr_type} realm=${realm_num} count=${actv_realms}"
    zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_zone_activate -v -z \
			${device} ${conv_lba} ${nr_conv_zones} ${cmr_type}
fi
