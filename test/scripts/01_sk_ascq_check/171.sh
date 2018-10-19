#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016-2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

if [[ ${test_zone_type} == ${ZT_CONV} ]]; then
    zone_cond=${ZC_NOT_WP}
    start_cond="NOT_WP"
else
    zone_cond=${ZC_EMPTY}
    start_cond="EMPTY"
fi
zbc_test_init $0 "WRITE zone in ${start_cond} condition when (max_open - ${test_ozr_reserve:-0}) zones are Explicitly-Open (type=${test_zone_type:-${ZT_SEQ}})" $*

# Set expected error code
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

zbc_test_have_max_open_or_NA

# If ${test_ozr_reserve} is set then the OZR check should succeed
want_open=$(( ${max_open} - ${test_ozr_reserve:-0} ))

# Let us assume that all the available Write Pointer zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Select ${seq_zone_type} and get ${nr_avail_seq_zones}
zbc_test_get_seq_type_nr

if [ ${want_open} -ge ${nr_avail_seq_zones} ]; then
    zbc_test_print_not_applicable \
	"Not enough (${nr_avail_seq_zones}) available zones" \
	"of type ${seq_zone_type} to exceed max_open (${max_open})"
fi

# Make sure we have available a writable zone that is not OPEN
zbc_test_search_zone_cond_or_NA "${zone_cond}"

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start testing
# Explicitly open ${want_open} zones of type ${seq_zone_type}
zbc_test_open_nr_zones ${seq_zone_type} ${want_open}
if [ $? -ne 0 ]; then
    zbc_test_fail_exit "open_nr_zones ${seq_zone_type} ${want_open}"
fi

# Get a writable zone (of any type) that is not OPEN
zbc_test_search_zone_cond "${zone_cond}"
if [ $? -ne 0 ]; then
    zbc_test_fail_exit "WARNING: Unexpected failure to find writable zone"
fi

# ${seq_zone_type} is SWR or SWP -- we just opened ${want_open} zones of this type.
# ${target_slba}/${target_type} is any type of zone, EMPTY if a write pointer zone --
#	we are about to attempt to write to it.
# If both zone types participate in the Open Zone Resources (OZR) protocol,
#	then the write is expected to fail "Insufficient-zone-resources".

# Attempt to write to the target LBA in the non-OPEN zone
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}

zbc_test_get_sk_ascq
if [[ ${seq_zone_type} != @(${ZT_W_OZR}) || ${target_type} != @(${ZT_W_OZR}) ]]; then
    # At least one of the zone types does not participate in the OZR protocol
    zbc_test_check_no_sk_ascq "${want_open} * ${seq_zone_type} + ${target_type}"
elif [ ${test_ozr_reserve:-0} -gt 0 ]; then
    # We still had available OZR resources when we tried the final operation
    zbc_test_check_no_sk_ascq "${want_open} * ${seq_zone_type} + ${target_type}"
else
    # Both Zone types participate in the OZR protocol
    zbc_test_check_sk_ascq "${want_open} * ${seq_zone_type} + ${target_type}"
fi
