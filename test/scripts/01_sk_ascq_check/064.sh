#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "READ access write pointer zone LBAs starting after write pointer" $*

# Get drive information
zbc_test_get_device_info

expected_sk="Illegal-request"
expected_asc="Attempt-to-read-invalid-data"	# read across WP

# Search target LBA
zbc_test_get_wp_zone_or_NA "${ZC_NON_FULL}"
target_lba=${target_ptr}

# Start testing
# Write a block starting at the write pointer
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}
if [ $? -ne 0 ]; then
    printf "\nInitial write failed"
else
    # Attempt to read an LBA starting beyond the write pointer
    zbc_test_run ${bin_path}/zbc_test_read_zone -v ${device} $(( ${target_lba} + ${lblk_per_pblk} + 1 )) 1
fi

# Check result
zbc_test_get_sk_ascq

if [[ ${unrestricted_read} -ne 0 || ${target_type} != @(${ZT_RESTRICT_READ_GE_WP}) ]]; then
    zbc_test_check_no_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
else
    zbc_test_check_sk_ascq "zone_type=${target_type} URSWRZ=${unrestricted_read}"
fi

# Post process
rm -f ${zone_info_file}
