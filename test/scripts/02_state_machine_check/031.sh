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

. scripts/zbc_test_lib.sh

zbc_test_init $0 "FINISH_ZONE implicit open to full (ALL bit set)" $*

expected_cond="0xe"

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_cond "0x2|0x3" "0x1"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No EMPTY Sequential zones"
fi
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${sect_per_pblk}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"

zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} -1

# Get SenseKey, ASC/ASCQ
zbc_test_get_sk_ascq

# Get zone information
zbc_test_get_zone_info "5"

# Get target zone condition
zbc_test_get_target_zone_from_slba ${target_lba}

# Check result
zbc_test_check_zone_cond

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_lba}

rm -f ${zone_info_file}
