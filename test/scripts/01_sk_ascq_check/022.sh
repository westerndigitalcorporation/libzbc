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

zbc_test_init $0 "OPEN_ZONE insufficient zone resources (ALL bit set)" $*

# Set expected error code
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

if [ ${max_open} -eq -1 ]; then
    zbc_test_print_not_applicable "max_open not reported"
fi

if [ ${device_model} = "Host-aware" ]; then
    zbc_test_print_not_applicable "Device is Host-aware"
fi

zone_type="0x2"

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Get zone information
zbc_test_get_zone_info

# Get the number of active zones of the zone_type
nr_active_zones_of_type=`zbc_zones | zbc_zone_filter_in_type "${zone_type}" | zbc_zone_filter_out_cond "0xc|0xd|0xe|0xf" | wc -l`

if [ ${max_open} -ge ${nr_active_zones_of_type} ]; then
    zbc_test_print_not_applicable "Not enough active zones of type ${zone_type}: (max_open=${max_open}) >= (nr_active_zones_of_type=${nr_active_zones_of_type})"
fi

# Create closed zones
declare -i count=0
for i in `seq $(( ${max_open} + 1 ))`; do

    # Get zone information
    zbc_test_get_zone_info

    # Search target LBA
    zbc_test_search_vals_from_zone_type_and_cond ${zone_type} "0x1"
    target_lba=${target_slba}

    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 8
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "Initial WRITE failed, zone_type=${target_type}"

    zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_lba}
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "Zone CLOSE failed, zone_type=${target_type}"

done

# Start testing
# Create more closed zones than we can have open at one time
zbc_test_close_nr_zones $(( ${max_open} + 1 ))

# Now try to open all the closed zones
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} -1

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
rm -f ${zone_info_file}

