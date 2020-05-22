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

# Expecting success -- writing one of the max_open zones to FULL should free an OZR slot
zbc_test_init $0 "OPEN an EMPTY zone OK after OPEN max_open and writing one of them to FULL" $*

# Get drive information
zbc_test_get_device_info

zbc_test_have_max_open_or_NA

# Let us assume that all the available Write Pointer zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Select ${seq_zone_type} and get ${nr_avail_seq_zones}
zbc_test_get_seq_type_nr
if [ ${max_open} -ge ${nr_avail_seq_zones} ]; then
    zbc_test_print_not_applicable "Not enough (${nr_avail_seq_zones}) available zones" \
				  "of type ${seq_zone_type} to exceed max_open (${max_open})"
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start testing
# Explicitly open ${max_open} zones of type ${seq_zone_type}
zbc_test_open_nr_zones ${seq_zone_type} ${max_open}
if [ $? -ne 0 ]; then
    zbc_test_fail_exit "open_nr_zones ${seq_zone_type} ${max_open}"
fi

# Write one of the OPEN zones to FULL
zbc_test_search_target_zone_from_type_and_cond ${seq_zone_type} ${ZC_EOPEN}
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${target_size}

# Find one more zone to try, which will exceed max_open iff the zone we wrote is still open
zbc_test_search_target_zone_from_type_and_cond ${seq_zone_type} ${ZC_EMPTY}
if [ $? -ne 0 ]; then
    # This should not happen because we counted enough zones above
    zbc_test_fail_exit \
	"WARNING: Expected EMPTY zone of type ${seq_zone_type} could not be found"
fi

# Now open one more zone
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_slba}

# Check result -- expected to succeed
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq "(${max_open} + 1) * ${seq_zone_type}"
