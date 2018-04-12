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

zbc_test_init $0 "OPEN_ZONE command completion" $*

expected_sk=""
expected_asc=""

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

if [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

# Search target LBA
target_lba="0"
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 -a "${zone_activation_device}" != "0" ]; then
    zbc_test_print_not_applicable
fi
target_lba=$(( ${target_slba} ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_lba}
# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_lba}
rm -f ${zone_info_file}

# Check failed
zbc_test_check_failed

