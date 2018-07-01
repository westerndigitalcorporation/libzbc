#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE across the End of Medium" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Logical-block-address-out-of-range"

# Get drive information
zbc_test_get_device_info

target_lba=${max_lba}

zbc_test_get_zone_info

zbc_test_get_target_zone_from_slba ${last_zone_lba}

# If the zone is inactive, our initial reset and write are expected to fail
zbc_write_check_available ${target_cond}

# Start testing
if [[ ${target_type} == @(${ZT_WP}) ]]; then
    # First get the write pointer close to the boundary
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${last_zone_lba}
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${last_zone_lba} $(( ${last_zone_size} - ${lblk_per_pblk} ))
fi

# Attempt to write across the End of Medium
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} $(( 2 * ${lblk_per_pblk} ))

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
