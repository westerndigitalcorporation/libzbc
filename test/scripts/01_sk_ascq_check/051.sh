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

zbc_test_init $0 "RESET_WRITE_PTR CMR zone" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_ignored_cond "0x1|0x4" "0xc|0xd|0xf"

if [ $? -gt 0 ]; then
    zbc_test_print_not_applicable "No CMR zone is available"
fi

target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq

if [ ${target_type} = "0x1" ]; then
	zbc_test_check_sk_ascq		# RESET_WP not allowed on conventional zone
else
	zbc_test_check_no_sk_ascq	# RESET_WP allowed on other types of zones
fi

# Post process
rm -f ${zone_info_file}
