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

zbc_test_init $0 "OPEN_ZONE invalid zone start lba" $*

expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

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
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No EMPTY sequential zones"
fi
target_lba=$(( ${target_slba} + 1 ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
rm -f ${zone_info_file}

