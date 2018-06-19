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

zbc_test_init $0 "Run ZBC test on mixed conventional-sequential device" $*

ZBC_TEST_LOG_PATH_BASE=${2}/zonemix

zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "Sequential zones are not supported by the device"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Convert roughly half of the realms to sequential -
# Find a conventional realm that is convertible to sequential
zbc_test_search_realm_by_type_and_cvt "${ZT_NON_SEQ}" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently conventional and convertible to sequential"
fi

# Find the total number of convertible realms
zbc_test_count_cvt_realms		# nr_realms
zbc_test_count_cvt_to_seq_realms
if [ $nr_cvt_to_seq_realms -eq 0 ]; then
    # This should not happen because we found one just above
    zbc_test_print_failed "No realms are convertible to sequential"
fi
if [ $(expr "${realm_num}" + "${nr_cvt_to_seq_realms}") -gt ${nr_realms} ]; then
    nr_cvt_to_conv_realms=$(expr "${nr_realms}" - "${realm_num}")
fi

# Take the first half
nr=$(( nr_cvt_to_seq_realms/2 ))
if [ ${nr} -eq 0 ]; then
    nr=1
fi

# Convert the realms to the configuration for the run we invoke below
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${realm_num} ${nr} ${smr_type}
if [ $? -ne 0 ]; then
    printf "\nFailed to convert device to intended test configuration ${realm_num} ${nr} ${smr_type}"
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
