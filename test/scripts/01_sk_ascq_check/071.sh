#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE write-pointer zone boundary violation (cross-zone)" $*
# Also checks that cross-zone writes succeed on SWP and WPC zones

# Get drive information
zbc_test_get_device_info

if [ -n "${test_zone_type}" ]; then
    zone_type=${test_zone_type}
else
    zone_type="0x2|0x3"
fi

if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"		# write cross-zone

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "${ZC_EMPTY}"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone is of type ${zone_type} and EMPTY"
fi

nio=$(( (${target_size} - 1) / 8 ))

# Start testing
# Write the zone from empty to within a few LBA of the end
zbc_test_run ${bin_path}/zbc_test_write_zone -v -n ${nio} ${device} ${target_slba} 8
if [ $? -ne 0 ]; then
    #XXX How could this happen?  What error should be expected?
    printf "\nInitial write zone failed (target_size=${target_size} zone_type=${target_type})"
else
    # Attempt to write through the remaining LBA of the zone and cross over into the next zone
    target_lba=$(( ${target_slba} + ${nio} * 8 ))
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 16
fi

# Check result
zbc_test_get_sk_ascq

#XXX Customer requires that 0x4 succeed here -- fix SPEC to match
if [[ ${target_type} == @(0x3|0x4) ]]; then
    # Permissible to write cross-zone in WPC and SWP zones
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    # Not allowed to write cross-zone in SWR zones
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} $(( ${target_slba} + ${target_size} ))
rm -f ${zone_info_file}

