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

zbc_test_init $0 "Run ZBC test on another mixed CONV or SOBR/SWR or SWP device" $*

ZBC_TEST_LOG_PATH_BASE=${2}/zonemix2

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

# 24 assigned to conventional + enough SWR or SWP for OZR tests
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq" "NOFAULTY"
first=${realm_num}

want_realms=$(( ${first} + 24 + 1 + ${max_open} / $(zbc_realm_smr_len) ))
if [ ${nr_realms} -lt ${want_realms} ]; then
    zbc_test_print_not_applicable \
	"Not enough realms (${nr_realms}) to run this configuration (${want_realms})"
fi

if [ ${max_act} != "unlimited" ]; then
    zbc_test_print_not_applicable "This test requires unlimited activation size"
fi

# Configure the zone realms, with all realms freshly activated except 0 and 5.
# This ends up with all zone realms sequential except realms 0-22 and the last realm.

activate_fail()
{
    printf "\n${0}: Failed to activate device realms to intended test configuration ($*)\n"
    zbc_test_dump_zone_realm_info
    zbc_test_dump_zone_info
    exit
}

# Make sure all zones are EMPTY for activation
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1

# Activate everything as CONV or SOBR, except don't touch Realms 0 or 5.

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${first} + 1 )) 4 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${first} + 1 )) 4 ${cmr_type} (1)"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${first} + 6 )) $(( ${nr_realms} - ${first} - 6 )) ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${first} + 6 )) $(( ${nr_realms} - 6 )) ${cmr_type}"
fi

# Activate everything as SWR or SWP, except don't touch Realms 0 or 5.

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${first} + 1 )) 4 ${smr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${first} + 1 )) 4 ${smr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${first} + 6 )) $(( ${nr_realms} - ${first} - 6 )) ${smr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${first} + 6 )) $(( ${nr_realms} - 6 )) ${smr_type}"
fi

# Activate the first 23 Realms as CONV or SOBR, except don't touch Realms 0 or 5.

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${first} + 1 )) 4 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${first} + 1 )) 4 ${cmr_type} (2)"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${first} + 6 )) $(( ${first} + 17 )) ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${first} + 6 )) 17 ${cmr_type}"
fi

# Activate the last three realms as CONV or SOBR

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${nr_realms} - 3 )) 3 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${nr_realms} - 3 )) 3 ${cmr_type}"
fi

# Pass the batch_mode flag through to the run we invoke below
arg_b=""
if [ ${ZBC_TEST_BATCH_MODE} -ne 0 ] ; then
    arg_b="-b"
fi

arg_a=""
if [ ${ZBC_TEST_FORCE_ATA} ]; then
    arg_a="-a"
fi

arg_w=""
if [ ${ZBC_ACCEPT_ANY_FAIL} ]; then
    arg_w="-w"
fi

arg_l=""
if [ ${RUN_ACTIVATIONS_ONLY} ]; then
    arg_l="-l"
fi
arg_x=""
if [ ${ZBC_RUN_EXTENDED_TESTS} ]; then
    arg_x="-x"
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start ZBC test
zbc_test_meta_run ./zbc_xmr_test.sh ${arg_a} ${arg_b} ${arg_w} ${arg_x} ${arg_l} -n ${eexec_list} ${cskip_list} ${device}
if [ $? -ne 0 ]; then
    sk="04.040 fail -- log path ${ZBC_TEST_LOG_PATH_BASE}"
    asc="child test of 04.040 failed $?"
fi

# Check result
zbc_test_check_no_sk_ascq
