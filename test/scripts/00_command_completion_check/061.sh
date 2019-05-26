#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2019, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE command completion (vector I/O)" $*

# Set expected error code
expected_sk=""
expected_asc=""

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
zbc_test_get_target_zone_from_type_and_ignored_cond ${zone_type} "0xe"
target_lba=$(( ${target_ptr} ))

# Start testing, write a specific byte pattern
zbc_test_run ${bin_path}/zbc_test_write_zone -v -vio 8 -p 0xaa ${device} ${target_lba} ${lblk_per_pblk}

# Check result
zbc_test_fail_if_sk_ascq "WRITE failed, zone_type=${zone_type}"

# Make sure we read back the same byte pattern
zbc_test_run ${bin_path}/zbc_test_read_zone -v -vio 8 -p 0xaa ${device} ${target_lba} ${lblk_per_pblk}

#Check the result again. The read will fail if there is a mismatch
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
rm -f ${zone_info_file}

# Check failed
zbc_test_check_failed

