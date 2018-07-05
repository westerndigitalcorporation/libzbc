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

zbc_test_init $0 "RESET_WRITE_PTR ZONE-ID ignored when ALL bit is set" $*

expected_cond="${ZC_EMPTY}"

# Get drive information
zbc_test_get_device_info

zbc_test_search_wp_zone_cond_or_NA ${ZC_EMPTY}
target_lba=${target_slba}

# Start testing
# Make the zone non-empty
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}

# Attempt RESET ALL, specifying a bad LBA which is expected to be IGNORED
zbc_test_run ${bin_path}/zbc_test_reset_zone --ALL ${device} $(( ${max_lba} + 2 ))

# Get SenseKey, ASC/ASCQ
zbc_test_get_sk_ascq

# Get zone information
zbc_test_get_zone_info

# Get target zone condition
zbc_test_get_target_zone_from_slba ${target_lba}

# Check result
zbc_test_check_zone_cond "zone_type=${target_type}"

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_lba}

rm -f ${zone_info_file}
