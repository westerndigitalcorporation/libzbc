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

zbc_test_init $0 "WRITE unaligned ${device_model} ${test_zone_type} starting above write pointer" $*

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
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting and ending above WP

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_ignored_cond ${zone_type} "0xe|0xc|0xd|0xf"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone is of type ${zone_type} and active but not FULL"
fi
target_lba=$(( ${target_ptr} + 1 ))	# unaligned write starting above WP

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq

if [ ${zone_type} = "0x3" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}

