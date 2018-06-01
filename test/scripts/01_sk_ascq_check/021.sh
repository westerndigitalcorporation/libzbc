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

zbc_test_init $0 "OPEN_ZONE insufficient zone resources" $*

# Set expected error code
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

if [ ${max_open} -eq -1 ]; then
    zbc_test_print_not_applicable "max_open not reported"
fi

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Get zone information
zbc_test_get_zone_info

# See if there are any SWR zones at all
nr_SWR_zones=`zbc_zones | zbc_zone_filter_in_type ${ZT_SWR} | wc -l`
if [ ${nr_SWR_zones} -gt 0 ]; then
    seq_zone_type=${ZT_SWR}		# primary test
else
    # No SWR zones on the device -- use SWP (and expect a different result below)
    seq_zone_type=${ZT_SWP}		# fallback test
fi

# Get the number of available sequential zones of the type chosen
nr_avail_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${seq_zone_type}" \
			| zbc_zone_filter_in_cond "${ZC_EMPTY}" | wc -l`

if [ ${max_open} -ge ${nr_avail_seq_zones} ]; then
    zbc_test_print_not_applicable "Not enough (${nr_avail_seq_zones}) EMPTY zones" \
				  "of type ${seq_zone_type} to exceed max_open (${max_open})"
fi

# Start testing
# Explicitly open a total of ${max_open} sequential zones
zbc_test_open_nr_zones ${seq_zone_type} ${max_open}

# Update zone information
zbc_test_get_zone_info

# Find one more sequential zone to try, which would exceed max_open
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
if [ $? -ne 0 ]; then
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
    # This should not happen because we counted enough zones above
    zbc_test_print_not_applicable "WARNING: No EMPTY zone of type ${seq_zone_type} could be found"
fi

target_lba=${target_slba}

zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
if [[ ${seq_zone_type} != @(${ZT_W_OZR}) || ${target_type} != @(${ZT_W_OZR}) ]]; then
    zbc_test_check_no_sk_ascq "${max_open} * (seq_zone_type=${seq_zone_type}) + (zone_type=${target_type})"
else
    zbc_test_check_sk_ascq "${max_open} * (seq_zone_type=${seq_zone_type}) + (zone_type=${target_type})"
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
rm -f ${zone_info_file}
