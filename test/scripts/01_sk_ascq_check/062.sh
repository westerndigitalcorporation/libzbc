#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc.  All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

. ../zbc_test_lib.sh

zbc_test_init $0 $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"

zbc_test_info "READ conventional/sequential zones boundary violation..."

# Get drive information
zbc_test_get_drive_info

if [ ${device_model} = "Host-aware" ]; then
    zbc_test_print_not_applicable
    exit
fi

# Get zone information
zbc_test_get_zone_info

# Search target LBA
# Search last conventional zone info
zbc_test_search_last_zone_vals_from_zone_type "0x1"

func_ret=$?

if [ ${func_ret} -gt 0 ]; then
    zbc_test_print_not_applicable
    exit
fi

next_zone_slba=$(( ${target_slba} + ${target_size} ))

# Search first sequential zone info
zbc_test_search_vals_from_zone_type "0x2"
func_ret=$?

if [ ${func_ret} -gt 0 -o ${next_zone_slba} != ${target_slba} ]; then
    zbc_test_print_not_applicable
    exit
fi

target_lba=$(( ${target_slba} - 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_slba}
zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 2

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_write_ptr -v ${device} ${target_slba}
rm -f ${zone_info_file}

