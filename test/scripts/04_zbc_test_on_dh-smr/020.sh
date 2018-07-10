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

zbc_test_init $0 "Run ZBC test activated as a mixed CMR/SMR device" $*

ZBC_TEST_LOG_PATH_BASE=${2}/zonemix

zbc_test_get_device_info

# Get zone realm information
zbc_test_get_zone_realm_info

# Activate roughly half of the realms as SMR -
# Find a CMR realm that can be activated as SMR
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently CMR and can be activated as SMR"
fi

# Find the total number of realms that can be activated as SMR
if [ $nr_actv_as_seq_realms -eq 0 ]; then
    # This should not happen because we found one just above
    zbc_test_print_failed "WARNING: No realm can be activated as SMR"
fi

if [ $(expr "${realm_num}" + "${nr_actv_as_seq_realms}") -gt ${nr_realms} ]; then
    nr_actv_as_conv_realms=$(expr "${nr_realms}" - "${realm_num}")
fi

# Try to take the first half
nr=$(( nr_actv_as_seq_realms/2 ))
if [ ${nr} -eq 0 ]; then
    nr=1
fi

# If activation size is restricted, stay within the limit
#XXX There must be a better way to do this
max_act=`zbc_info ${device} | grep "Maximum number of zones to activate" | sed -e "s/.* //"`  #XXX
if [ ${max_act} != "unlimited" ]; then
    zbc_test_calc_nr_realm_zones ${realm_num} ${nr}
    while [ ${nr_seq_zones} -gt ${max_act} ]; do
	nr=$(( ${nr} / 2 ))
	if [ ${nr} -eq 0 ]; then
	    zbc_test_print_not_applicable "Cannot activate sequential zones (max_activate=${max_act})"
	fi
	zbc_test_calc_nr_realm_zones ${realm_num} ${nr}
    done
fi

# Activate the realms to the configuration for the run we invoke below
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${realm_num} ${nr} ${smr_type}
if [ $? -ne 0 ]; then
    printf "\nFailed to activate device realms to intended test configuration ${realm_num} ${nr} ${smr_type}\n"
    exit 1
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

# Start ZBC test on the mixed zone-type configuration
zbc_test_meta_run ./zbc_dhsmr_test.sh ${arg_a} ${arg_b} -n ${eexec_list} ${cskip_list} ${device}
if [ $? -ne 0 ]; then
    sk="fail -- log path ${ZBC_TEST_LOG_PATH_BASE}"
    asc="ZBC test 04.020 failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

# Post-process cleanup
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
