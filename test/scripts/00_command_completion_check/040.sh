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

zbc_test_init $0 "FINISH_ZONE command completion" $*

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_cond "${ZT_SEQ}" "${ZC_EMPTY}"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No EMPTY Sequential zones"
fi
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_lba}
rm -f ${zone_info_file}

# Check failed
zbc_test_check_failed
