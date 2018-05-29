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

zbc_test_init $0 "WRITE unaligned crossing write pointer" $*

# Get drive information
zbc_test_get_device_info

zone_type=${test_zone_type:-"0x2|0x3"}
if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting below and ending above WP

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_ignored_cond ${zone_type} "0xc|0xd|0xe|0xf"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No write-pointer zone is of type ${zone_type} and active but NON-FULL"
fi
target_lba=${target_ptr}

# Start testing
# Write 4 LBA starting at the write pointer
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 4
if [ $? -ne 0 ]; then
    printf "\nInitial write failed"
else
    # Attempt to write 8 LBA from the same starting LBA, overwriting the 4 just written
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 8
fi

# Check result
zbc_test_get_sk_ascq

if [[ ${target_type} == @(0x3|0x4) ]]; then
    zbc_test_check_no_sk_ascq zone_type=${target_type}
else
    zbc_test_check_sk_ascq zone_type=${target_type}
fi

# Post process
rm -f ${zone_info_file}

