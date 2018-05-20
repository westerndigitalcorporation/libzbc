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

zbc_test_init $0 "WRITE insufficient zone resources" $*

# Set expected error code
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

if [ ${max_open} -eq -1 ]; then
    zbc_test_print_not_applicable "max_open not reported"
fi

if [ -n "${test_zone_type}" ]; then
    zone_type=${test_zone_type}
else
    zone_type="0x2|0x3"
fi

if [ ${zone_type} = "0x1" ]; then
    zbc_test_print_not_applicable "Zone type ${zone_type} is not a write-pointer zone type"
fi

# Get zone information
zbc_test_get_zone_info

# Get the number of active zones of the zone_type
nr_active_zones_of_type=`zbc_zones | zbc_zone_filter_in_type "${zone_type}" | zbc_zone_filter_out_cond "0xc|0xd|0xe|0xf" | wc -l`

if [ ${max_open} -ge ${nr_active_zones_of_type} ]; then
    zbc_test_print_not_applicable "Not enough active zones of type ${zone_type}: (max_open=${max_open}) >= (nr_active_zones_of_type=${nr_active_zones_of_type})"
fi

# Open zones
zbc_test_open_nr_zones ${max_open}

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 ]; then
    printf "\nUnexpected failure to find zone of type ${zone_type} after counting enough zones"
else
    target_lba=${target_slba}

    # Start testing
    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} 8

    # Check result
    zbc_test_get_sk_ascq

    if [[ ${target_type} == @(0x3|0x4) ]]; then
        zbc_test_check_no_sk_ascq "zone_type=${target_type}"
    else
        zbc_test_check_sk_ascq "zone_type=${target_type}"
    fi
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

rm -f ${zone_info_file}

