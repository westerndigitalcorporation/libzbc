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

zbc_test_init $0 "WRITE cross-zone OPEN->INACTIVE (type=${test_zone_type:=${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

# Get a pair of zones with the second one INACTIVE
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
zbc_test_search_zone_pair_or_NA ${test_zone_type} ${ZC_EMPTY} ${ZC_INACTIVE}
target_lba=$(( ${target_slba} + ${target_size} - ${lblk_per_pblk} ))

expected_sk="Data-protect"
expected_asc="Zone-is-inactive"				# write inactive zone
if [[ ${target_type} == @(${ZT_DISALLOW_WRITE_XZONE}) ]]; then
    expected_sk="Illegal-request"
    expected_asc="Write-boundary-violation"		# write cross-zone
fi

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} \
			$(( ${target_slba} + ${target_size} ))

# Start testing
# Get the write pointer close to the boundary
zbc_test_run ${bin_path}/zbc_test_write_zone -v \
		${device} ${target_slba} $(( ${target_size} - ${lblk_per_pblk} ))

# Write across the zone boundary and into the INACTIVE zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v \
		${device} ${target_lba} $(( ${lblk_per_pblk} * 2 ))

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq "zone_type=${target_type}"
