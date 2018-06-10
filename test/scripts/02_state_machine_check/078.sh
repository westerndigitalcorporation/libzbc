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

zbc_test_init $0 "WRITE closed to full" $*

expected_cond="0xe"

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

zbc_test_get_seq_zone_set_cond_or_NA "IOPENL"
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_lba}

if [ -z "${sk}" ]; then
    # Write the rest of the zone
    resid=$(( ${target_size} - ${sect_per_pblk} ))
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} \
			$(( ${target_lba} + ${sect_per_pblk} )) ${resid}
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "WRITE failed, zone_type=${target_type}"

    if [ -z "${sk}" ]; then
	zbc_test_get_zone_info
	zbc_test_search_vals_from_slba ${target_lba}
	zbc_test_check_zone_cond
    fi
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

rm -f ${zone_info_file}
