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

zbc_test_init $0 "WRITE implicit open to full starting below write pointer (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

# Search target LBA
zbc_test_get_wp_zones_cond_or_NA "IOPENH"
target_lba=$(( ${target_ptr} - ${lblk_per_pblk} ))
initial_wp=${target_ptr}
initial_cond=${target_cond}

write_size=$(( 2 * ${lblk_per_pblk} ))

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting and ending above WP

# Specify post process
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
# Write the rest of the zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${write_size}

# Check result
zbc_test_get_sk_ascq
zbc_test_get_target_zone_from_slba ${target_slba}

if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_LT_WP}) ]]; then
    expected_cond="${ZC_FULL}"
    zbc_test_check_zone_cond "zone_type=${target_type}"
else
    # Write should have failed -- check WP is unmodified
    expected_cond=${initial_cond}
    zbc_test_check_sk_ascq_zone_cond_wp ${initial_wp} "zone_type=${target_type}"
fi
