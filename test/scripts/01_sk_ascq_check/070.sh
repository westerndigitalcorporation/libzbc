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

zbc_test_init $0 "WRITE unaligned starting above write pointer" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Unaligned-write-command"		# Write starting and ending above WP

# Search target LBA
zbc_test_get_wp_zone_or_NA "${ZC_NON_FULL}"
target_lba=$(( ${target_ptr} + 1 ))	# unaligned write starting above WP

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${sect_per_pblk}

# Check result
zbc_test_get_sk_ascq

if [[ ${target_type} != @(${ZT_DISALLOW_WRITE_GT_WP}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type}"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
rm -f ${zone_info_file}
