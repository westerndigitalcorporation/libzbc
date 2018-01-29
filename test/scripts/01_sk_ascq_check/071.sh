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

zbc_test_init $0 "WRITE sequential zone boundary violation" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Write-boundary-violation"

# Get drive information
zbc_test_get_device_info

if [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_ignored_cond ${zone_type} "0xe|0xc"

# Start testing
nio=$(( (${target_size} - 7) / 8 ))

zbc_test_run ${bin_path}/zbc_test_write_zone -v -n ${nio} ${device} ${target_slba} 8
if [ $? -eq 0 ]; then
    target_lba=$(( ${target_slba} + ${nio} * 8 ))
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 16
else
    printf "\nInitial write zone failed"
fi

# Check result
zbc_test_get_sk_ascq

if [ ${device_model} = "Host-aware" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}

