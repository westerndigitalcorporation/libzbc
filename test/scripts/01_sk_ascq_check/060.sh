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

zbc_test_init $0 "READ access write pointer zone LBAs across write pointer" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# read above WP

# Search target LBA
zbc_test_get_wp_zone_or_NA "${ZC_NON_FULL}"
target_lba=${target_ptr}

# Start testing
# Write ${sect_per_pblk} LBA starting at the write pointer
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${sect_per_pblk}
if [ $? -ne 0 ]; then
    printf "\nInitial write failed"
else
    # Attempt to read more LBA across and beyond the ones we just wrote
    zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} ${target_lba} $(( ${sect_per_pblk} + 1 ))
fi

# Check result
zbc_test_get_sk_ascq

if [[ ${unrestricted_read} -ne 0 || ${target_type} != @(${ZT_RESTRICT_READ_GE_WP}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}
rm -f ${zone_info_file}
