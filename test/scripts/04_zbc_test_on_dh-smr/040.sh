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

zbc_test_init $0 "Run ZBC test on another mixed conventional-sequential device" $*

ZBC_TEST_LOG_PATH_BASE=${2}/zonemix2

zbc_test_get_device_info

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the total number of zone realms
zbc_test_count_zone_realms		# nr_realms

if [ ${nr_realms} -le 6 ]; then
    zbc_test_print_not_applicable "Not enough realms to run this configuration"
fi

# Configure the zone realms, with all realms freshly activated SMR except 0 and 5.
# This ends up with all zone realms sequential except realms 0-20 and the last realm.

activate_fail()
{
    printf "\nFailed to activate device realms to intended test configuration ($*)"
    exit 1
}

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "1 4 ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 $(( ${nr_realms} - 6 )) ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "6 $(( ${nr_realms} - 6 )) ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 ${smr_type}
if [ $? -ne 0 ]; then
    activate_fail "1 4 ${smr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 $(( ${nr_realms} - 6 )) ${smr_type}
if [ $? -ne 0 ]; then
    activate_fail "6 $(( ${nr_realms} - 6 )) ${smr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "1 4  ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 17 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "6 5 ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} $(( ${nr_realms} - 1 )) 1 ${cmr_type}
if [ $? -ne 0 ]; then
    activate_fail "$(( ${nr_realms} - 1 )) 1 ${cmr_type}"
fi

# Pass the batch_mode flag through to the run we invoke below
arg_b=""
if [ ${batch_mode} -ne 0 ] ; then
    arg_b="-b"
fi

arg_a=""
if [ "${ZBC_TEST_FORCE_ATA}" = "ATA" ]; then
    arg_a="-a"
fi

# Start ZBC test
zbc_test_meta_run ./zbc_dhsmr_test.sh ${arg_a} ${arg_b} -n ${eexec_list} ${cskip_list} ${device}
if [ $? -ne 0 ]; then
    sk="fail -- log path ${ZBC_TEST_LOG_PATH_BASE}"
    asc="ZBC test 04.040 failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

# Post-process cleanup
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
