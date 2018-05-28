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

zbc_test_init $0 "Run ZBC test on mixed CMR-SMR device" $*

export ZBC_TEST_LOG_PATH_BASE=${2}/zonemix

zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "Neither SWR nor SWP zones are supported by the device"
fi

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Convert roughly half of the domains to SMR -
# Find a CMR domain that is convertible to SMR
zbc_test_search_domain_by_type_and_cvt "0x1|0x4" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently CMR and convertible to SMR"
fi

# Find the total number of convertible domains
zbc_test_count_cvt_domains		# nr_domains
zbc_test_count_cvt_to_seq_domains
if [ $nr_cvt_to_seq_domains -eq 0 ]; then
    zbc_test_print_failed
fi
if [ $(expr "${domain_num}" + "${nr_cvt_to_seq_domains}") -gt ${nr_domains} ]; then
    nr_cvt_to_conv_domains=$(expr "${nr_domains}" - "${domain_num}")
fi

# Take the first half
nr=$(( nr_cvt_to_seq_domains/2 ))
if [ ${nr} -eq 0 ]; then
    nr=1
fi

# Convert the domains to the configuration for the run we invoke below
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${device} ${domain_num} ${nr} ${smr_type}
if [ $? -ne 0 ]; then
    printf "\nFailed to convert device to intended test configuration ${domain_num} ${nr} ${smr_type}"
    exit 1
fi

# Pass the batch_mode flag through to the run we invoke below
arg_b=""
if [ ${batch_mode} -ne 0 ] ; then
    arg_b="-b"
fi

# Start ZBC test on the mixed zone-type configuration
zbc_test_meta_run ./zbc_dhsmr_test.sh ${arg_b} -n ${eexec_list} ${device}
if [ $? -ne 0 ]; then
    sk="fail"
    asc="ZBC test 04.020 failed"
fi

# Check result
zbc_test_check_no_sk_ascq

# Check failed
zbc_test_check_failed

# Post-process cleanup
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
