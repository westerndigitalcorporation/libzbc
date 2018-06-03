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

if [ ${max_open} -eq 0 ]; then
    if [ "${device_model}" != "Host-managed" ]; then
    	zbc_test_print_not_applicable "Device is not Host-managed"
    fi
    zbc_test_print_not_applicable "max_open reported as zero"
fi

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Get zone information
zbc_test_get_zone_info

# Get the number of available EMPTY SWR zones
nr_avail_SWR_zones=`zbc_zones | zbc_zone_filter_in_type ${ZT_SWR} \
			| zbc_zone_filter_in_cond "${ZC_EMPTY}" | wc -l`

if [ ${max_open} -ge ${nr_avail_SWR_zones} ]; then
    zbc_test_print_not_applicable "Not enough (${nr_avail_SWR_zones}) available SWR zones" \
				  "to exceed max_open (${max_open})"
fi

# Start testing
# Create more closed zones than the device is capable of having open at a time
declare -i count=0
for i in `seq $(( ${max_open} + 1 ))`; do

    # Get zone information
    zbc_test_get_zone_info

    # Search target LBA
    zbc_test_search_vals_from_zone_type_and_cond "${ZT_SWR}" "${ZC_EMPTY}"
    if [ $? -ne 0 ]; then
        zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
        # This should not happen because we counted enough zones above
        zbc_test_print_not_applicable "WARNING: No EMPTY SWR zone could be found"
    fi

    target_lba=${target_slba}

    zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${sect_per_pblk}
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "Initial WRITE failed"

    zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_lba}
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq "Zone CLOSE failed"

done

# Now try to open ALL closed zones
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} -1

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
rm -f ${zone_info_file}

