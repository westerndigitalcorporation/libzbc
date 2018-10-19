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

zbc_test_init $0 "Run ZBC test on a mixed CONV or SOBR/SWR/SWP device" $*

ZBC_TEST_LOG_PATH_BASE=${2}/zonemix3

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

if [ ${seq_req_zone} -eq 0 -o ${seq_pref_zone} -eq 0 ]; then
    zbc_test_print_not_applicable "Device does not have both SWR and SWP zones"
fi

# 24 assigned to conventional + enough SWR or SWP for OZR tests
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "seq" "NOFAULTY"

want_realms=$(( 24 + 1 + ${max_open} / $(zbc_realm_smr_len) ))
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
		${device} 1 4 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "1 4 ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} 6 $(( ${nr_realms} - 6 )) ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "6 $(( ${nr_realms} - 6 )) ${cmr_type}"
fi

# Activate everything as SWR or SWP, except don't touch Realms 0 or 5.

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} 1 4 ${smr_type}
if [ $? -ne 0 ]; then
    activate_fail "1 4 ${smr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} 6 $(( ${nr_realms} / 2 - 3 )) "seq"
if [ $? -ne 0 ]; then
    activate_fail "6 $(( ${nr_realms} / 2 - 3 )) ${ZT_SWR}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${nr_realms} / 2 + 3 )) $(( ${nr_realms} / 2 - 3 )) "seqp"
if [ $? -ne 0 ]; then
    activate_fail "$(( ${nr_realms} / 2 + 3 )) $(( ${nr_realms} / 2 - 3 )) ${ZT_SWP}"
fi

# Activate the first 23 Realms as CONV or SOBR, except don't touch Realms 0 or 5.

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} 1 4 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "1 4 ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} 6 17 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "6 17 ${cmr_type}"
fi

# Activate the last Realm as CONV or SOBR

zbc_test_run ${bin_path}/zbc_test_zone_activate -v \
		${device} $(( ${nr_realms} - 1 )) 1 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${nr_realms} - 1 )) 1 ${cmr_type}"
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

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start ZBC test
zbc_test_meta_run ./zbc_xmr_test.sh ${arg_a} ${arg_b} ${arg_w} ${arg_l} -n ${eexec_list} ${cskip_list} ${device}
if [ $? -ne 0 ]; then
    sk="04.040 fail -- log path ${ZBC_TEST_LOG_PATH_BASE}"
    asc="child test of 04.050 failed $?"
fi

# Check result
zbc_test_check_no_sk_ascq
