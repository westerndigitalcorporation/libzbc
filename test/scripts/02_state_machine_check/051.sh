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

# CLOSE transitions an OPEN zone to EMPTY if the WP is pointing to the start of the zone
zbc_test_init $0 "CLOSE_ZONE implicit open to empty (ALL bit set)" $*

expected_cond="0x1"

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

zbc_test_get_seq_zone_set_cond_or_NA "IOPENZ"
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} -1

# Get SenseKey, ASC/ASCQ
zbc_test_get_sk_ascq

# Get zone information
zbc_test_get_zone_info "1"

# Get target zone condition
zbc_test_search_vals_from_slba ${target_lba}

# Check result
zbc_test_check_zone_cond

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

rm -f ${zone_info_file}
