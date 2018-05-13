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
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE sequential zone boundary violation (cross-zone)" $*

# Get drive information
zbc_test_get_device_info

if [ -n "${test_zone_type}" ]; then
    zone_type=${test_zone_type}
elif [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer type"
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
if [ $? -eq 0 ]; then
    # Attempt to write through the remaining LBA of the zone and cross over into the next zone
    target_lba=$(( ${target_slba} + ${nio} * 8 ))
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 16
else
    #XXX How could this happen?  What error should be expected?
    printf "\nInitial write zone failed (target_size=${target_size})"
fi

# Check result
zbc_test_get_sk_ascq

#XXX Customer requires 0x4 succeed also
if [ ${zone_type} = "0x3" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}

