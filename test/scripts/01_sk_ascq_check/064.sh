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

zbc_test_init $0 "READ conventional/sequential zone type boundary violation" $*

# Set expected error code - ZBC 4.4.3.4.2 penultimate paragraph
expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# read cross-type

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search first sequential zone info
zbc_test_search_last_zone_vals_from_zone_type "0x2|0x3"
func_ret=$?
if [ ${func_ret} -ne 0 ]; then
    zbc_test_print_not_applicable "No sequential zone LBA space"
fi
reach_lba=${target_slba}

# Search last conventional zone info
zbc_test_search_last_zone_vals_from_zone_type "0x1|0x4"
func_ret=$?
if [ ${func_ret} -ne 0 ]; then
    zbc_test_print_not_applicable "No conventional zone LBA space"
fi

# Read from last conventional LBA to first sequential LBA
target_lba=$(( ${target_slba} + ${target_size} - 1 ))
_nlba=$(( ${reach_lba} - ${target_lba} + 1 ))

# Start testing
# Read across the boundary from conventional LBA into sequential LBA space
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} ${_nlba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
rm -f ${zone_info_file}

