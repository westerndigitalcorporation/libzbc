#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2023, Western Digital. All rights reserved.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "FINISH a CLOSED zone when (max_open - ${test_ozr_reserve:-0}) zones are Explicitly-Open (type=${test_zone_type:-${ZT_SEQ}})" $*

# Set expected error code -- ZBC 4.4.3.5.5.1(f)(B), ZBC 4.4.3.5.5.3
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Depending on internal design of the drive, FINISH at MAX OPEN may succeed.
alt_expected_sk=""
alt_expected_asc=""

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

# Get a zone of the test_zone_type
zbc_test_search_zone_cond ${ZC_EMPTY}
if [[ $? -ne 0 ]]; then
    zbc_test_print_not_applicable "No EMPTY zones of test_zone_type"
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start testing
# Set the zone to CLOSED
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_slba}

# Update zone information
zbc_test_get_zone_info

# Explicitly open ${want_open} EMPTY zones of type ${seq_zone_type}
zbc_test_open_nr_zones ${seq_zone_type} ${want_open}
if [ $? -ne 0 ]; then
    zbc_test_fail_exit "open_nr_zones ${seq_zone_type} ${want_open}"
fi

# Now attempt to finish the zone we closed above, to exceed the limit
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_slba}

# Check result
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
