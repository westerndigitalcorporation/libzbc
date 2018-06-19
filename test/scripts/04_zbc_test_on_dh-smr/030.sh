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

zbc_test_init $0 "Run ZBC test on converted back to conventional device" $*

ZBC_TEST_LOG_PATH_BASE=${2}/allcmr2

zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${wpc_zone} -ne 0 ]; then
    cmr_type="wpc"
else
    zbc_test_print_not_applicable "Conventional zones are not supported by the device"
fi

# Get zone realm information
zbc_test_get_zone_realm_info

# Find the first sequential realm that is convertible to conventional
zbc_test_search_realm_by_type_and_cvt "${ZT_SEQ}" "conv"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently sequential and convertible to conventional"
fi

# Find the total number of convertible realms
zbc_test_count_zone_realms		# nr_realms
zbc_test_count_cvt_to_zone_realms
if [ $nr_cvt_to_conv_realms -eq 0 ]; then
    # This should not happen because we found one just above
    zbc_test_print_failed "WARNING: No realms are convertible to conventional"
fi
if [ $(expr "${realm_num}" + "${nr_cvt_to_conv_realms}") -gt ${nr_realms} ]; then
    nr_cvt_to_conv_realms=$(expr "${nr_realms}" - "${realm_num}")
fi

# Convert the realms to the configuration for the run we invoke below
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${realm_num} ${nr_cvt_to_conv_realms} ${cmr_type}
if [ $? -ne 0 ]; then
    printf "\nFailed to convert device to intended test configuration ${realm_num} ${nr_cvt_to_conv_realms} ${cmr_type}"
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

# Start ZBC test
zbc_test_meta_run ./zbc_dhsmr_test.sh ${arg_a} ${arg_b} -n ${eexec_list} ${cskip_list} ${device}
if [ $? -ne 0 ]; then
    sk="fail -- log path ${ZBC_TEST_LOG_PATH_BASE}"
    asc="ZBC test 04.030 failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

# Post-process cleanup
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
