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

zbc_test_init $0 "Implicitly-Open SWR zones autoclose to free OZR for WRITE" $*

# Get drive information
zbc_test_get_device_info

zbc_test_have_max_open_or_NA

# Let us assume that all the available Write Pointer zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

if [ ${seq_req_zone} -eq 0 ]; then
    zbc_test_print_not_applicable "Device has no SWR zones"
fi

# Select ${seq_zone_type} and get ${nr_avail_seq_zones}
zbc_test_get_seq_type_nr

if [ ${max_open} -ge ${nr_avail_seq_zones} ]; then
    zbc_test_print_not_applicable "Not enough (${nr_avail_seq_zones}) available zones" \
				  "of type ${seq_zone_type} to exceed max_open (${max_open})"
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start testing
# Implicitly open ${max_open} zones of type ${seq_zone_type}
zbc_test_iopen_nr_zones ${seq_zone_type} ${max_open}
if [[ $? -ne 0 ]]; then
    zbc_test_fail_exit "write ${max_open} SWR zones"
fi

# Now get another zone and try a WRITE
zbc_test_search_seq_zone_cond ${ZC_EMPTY}
if [[ $? -ne 0 ]]; then
    zbc_test_fail_exit "WARNING: Cannot find expected sequential zone"
fi

zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq \
    "WRITE EMPTY SWR while ${max_open} SWR zones Implicitly-Open"
