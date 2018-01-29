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
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "OPEN_ZONE insufficient zone resources" $*

# Set expected error code
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

if [ ${device_model} = "Host-aware" ]; then
    zbc_test_print_not_applicable
fi

zone_type="0x2"

# Get zone information
zbc_test_get_zone_info

# Check the number of stasis zones
zbc_test_count_stasis_zones

# Check number of sequential zones
zbc_test_count_seq_zones

if [ ${max_open} -ge $((${nr_seq_zones} - ${nr_stasis_zones})) ]; then
    zbc_test_print_not_applicable
fi

# Open zones
zbc_test_open_nr_zones ${max_open}

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_search_vals_from_zone_type_and_cond ${zone_type} "0x1"
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} -1
rm -f ${zone_info_file}

