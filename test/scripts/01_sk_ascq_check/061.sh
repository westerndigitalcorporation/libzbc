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

zbc_test_init $0 "READ across ${device_model} ${test_zone_type} write-pointer zones (FULL->EMPTY)" $*

# Get drive information
zbc_test_get_device_info

if [ -n "${test_zone_type}" ]; then
    zone_type=${test_zone_type}
elif [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

expected_sk="Illegal-request"
expected_asc="Read-boundary-violation"		# read cross-zone

if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
elif [ ${zone_type} = "0x4" ]; then
    #XXX Customer requires to allow WPC cross-zone reads
    expected_asc="Attempt-to-read-invalid-data"	# because second zone has no data
    expected_asc="Read-boundary-violation"	# ZA-r4 SPEC
fi

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_ignored_cond ${zone_type} "0xe|0xc|0xd|0xf"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone of type ${zone_type} is active"
fi
target_lba=$(( ${target_slba} + ${target_size} - 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_slba}
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} $(( ${target_lba} + 1 ))
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq

if [ "${unrestricted_read}" = "1" -o ${zone_type} = "0x3" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
rm -f ${zone_info_file}

