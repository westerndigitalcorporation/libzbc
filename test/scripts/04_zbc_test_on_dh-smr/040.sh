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
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "Run ZBC test on another mixed CMR-SMR device" $*

export ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH}/04.040_mix2

zbc_test_reset_device

zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
	cmr_type="conv"
elif [ ${wpc_zone} -ne 0 ]; then
	cmr_type="wpc"
else
	zbc_test_print_not_applicable "Neither conventional nor WPC zones are supported by the device"
fi

if [ ${seq_req_zone} -ne 0 ]; then
	smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
	smr_type="seqp"
else
	zbc_test_print_not_applicable "Neither SWR nor SWP zones are supported by the device"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find the total number of convertible domains
zbc_test_count_cvt_domains		# nr_domains

if [ ${nr_domains} -le 6 ]; then
	zbc_test_print_not_applicable "Not enough domains to run this configuration"
fi

# Configure the conversion domains, with all domains freshly converted except 0 and 5.
# This ends up with all conversion domains SMR except domains 0-10 and the last domain.

activate_fail()
{
	printf "\nFailed to convert device to intended test configuration ($*)"
	exit 1
}

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 ${cmr_type}
if [ $? -ne 0 ]; then
	activate_fail "1 4 ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 $(( ${nr_domains} - 6 )) ${cmr_type}
if [ $? -ne 0 ]; then
	activate_fail "6 $(( ${nr_domains} - 6 )) ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 ${smr_type}
if [ $? -ne 0 ]; then
	activate_fail "1 4 ${smr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 $(( ${nr_domains} - 6 )) ${smr_type}
if [ $? -ne 0 ]; then
	activate_fail "6 $(( ${nr_domains} - 6 )) ${smr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 1 4 ${cmr_type}
if [ $? -ne 0 ]; then
	activate_fail "1 4  ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} 6 5 ${cmr_type}
if [ $? -ne 0 ]; then
	activate_fail "6 5 ${cmr_type}"
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} $(( ${nr_domains} - 1 )) 1 ${cmr_type}
if [ $? -ne 0 ]; then
	activate_fail "$(( ${nr_domains} - 1 )) 1 ${cmr_type}"
fi

# Pass the batch_mode flag through to the run we invoke below
arg_b=""
if [ ${batch_mode} -ne 0 ] ; then
	arg_b="-b"
fi

# Start ZBC test
zbc_test_meta_run ./zbc_test.sh ${arg_b} -n ${device}
if [ $? -ne 0 ]; then
    sk="fail"
    asc="ZBC test failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

# Post-process cleanup
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
zbc_test_reset_device
