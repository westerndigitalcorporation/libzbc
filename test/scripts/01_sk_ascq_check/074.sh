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

zbc_test_init $0 "WRITE physical sector unaligned write to sequential zone" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"

# Get drive information
zbc_test_get_device_info

# if device model is host-aware or physical block size == logical block size
# then it is "not reported"
if [ ${device_model} = "Host-aware" \
     -o $((physical_block_size)) -eq $((logical_block_size)) ]; then
    zbc_test_print_not_applicable
fi

# Get zone information
zone_type="0x2"
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_ignored_cond ${zone_type} "0xe"
target_lba=$((target_ptr))

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 1

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
rm -f ${zone_info_file}

