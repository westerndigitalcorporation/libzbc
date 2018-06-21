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

zbc_test_init $0 "FINISH zone in CLOSED condition when max_open zones are EXP_OPEN" $*

# Set expected error code -- ZBC 4.4.3.5.5.1(f)(B), ZBC 4.4.3.5.5.3
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

if [ ${max_open} -eq -1 ]; then
    zbc_test_print_not_applicable "Device does not report max_open"
fi

if [ ${max_open} -eq 0 ]; then
    if [ "${device_model}" != "Host-managed" ]; then
	zbc_test_print_not_applicable "Device is not Host-managed"
    fi
    zbc_test_print_not_applicable "WARNING: Device reports max_open as zero"
fi

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Get zone information
zbc_test_get_zone_info

# If there are no SWR zones, test with SWP zones instead
nr_SWR_zones=`zbc_zones | zbc_zone_filter_in_type ${ZT_SWR} | wc -l`
if [ ${nr_SWR_zones} -gt 0 ]; then
    seq_zone_type=${ZT_SWR}	# primary test using Sequential-write-required
else
    # No SWR zones on the device -- use SWP (and expect a different result below)
    seq_zone_type=${ZT_SWP}	# fallback test using Sequential-write-preferred
fi

# Get the number of available sequential zones of the type we are using
nr_avail_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${seq_zone_type}" \
			| zbc_zone_filter_in_cond "${ZC_EMPTY}" | wc -l`

if [ ${max_open} -ge ${nr_avail_seq_zones} ]; then
    zbc_test_print_not_applicable \
	"Not enough (${nr_avail_seq_zones}) available zones" \
	"of type ${seq_zone_type} to exceed max_open (${max_open})"
fi

# Start testing
# Get one of the sequential zones and set it to CLOSED
zbc_test_get_target_zone_from_type_and_cond ${seq_zone_type} ${ZC_EMPTY}
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} ${target_slba} ${lblk_per_pblk}
zbc_test_run ${bin_path}/zbc_test_close_zone ${device} ${target_slba}
finish_lba=${target_slba}

# Update zone information
zbc_test_get_zone_info

# Explicitly open ${max_open} EMPTY sequential zones of ${seq_zone_type}
zbc_test_open_nr_zones ${seq_zone_type} ${max_open}
if [ $? -ne 0 ]; then
    zbc_test_get_sk_ascq
    zbc_test_fail_if_sk_ascq
    zbc_test_print_failed "Failed to open_nr_zones ${seq_zone_type} ${max_open}"
else
    # Now attempt to finish the zone we closed above, to exceed the limit
    zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${finish_lba}

    # Check result
    zbc_test_get_sk_ascq
    if [[ ${seq_zone_type} != @(${ZT_W_OZR}) ]]; then
	# zone type does not participate in the OZR protocol
	zbc_test_check_no_sk_ascq "(${max_open}+1) * (seq_zone_type=${seq_zone_type})"
    else
	# Zone type participates in the OZR protocol
	zbc_test_check_sk_ascq "(${max_open}+1) * (seq_zone_type=${seq_zone_type})"
    fi
fi

# Post process
zbc_test_check_failed
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
rm -f ${zone_info_file}