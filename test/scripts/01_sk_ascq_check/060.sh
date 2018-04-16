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

zbc_test_init $0 "READ access sequential zone LBAs after write pointer" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"

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
target_ptr="0"
no_zones=0
zbc_test_search_vals_from_zone_type_and_ignored_cond ${zone_type} "0xe|0xc"
if [ $? -ne 0 -a "${zone_activation_device}" != "0" ]; then
    no_zones=1
    expected_sk="Aborted-command"
    expected_asc="Zone-is-inactive"
fi
target_lba=$(( ${target_ptr} ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 8
if [ $? -ne 0 ]; then
	if [ no_zones = 0 ]; then
	    printf "\nInitial write failed"
	fi
else
    zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} 9
fi

# Check result
zbc_test_get_sk_ascq

if [ ${no_zones} != 0 ]; then
    zbc_test_check_sk_ascq
elif [ "${unrestricted_read}" = "1" -o ${device_model} = "Host-aware" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}

