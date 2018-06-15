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

zbc_test_init $0 "OPEN_ZONE non-sequential zone" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get drive information
zbc_test_get_device_info

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_non_seq_zone_cond_or_NA "${ZC_NON_FULL}"
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
if [[ ${target_type} == @(${ZT_WPC}) ]]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
rm -f ${zone_info_file}
