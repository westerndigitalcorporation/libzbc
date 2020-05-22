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

# The zones process from CLOSED to FINISH one at a time, not exceeding max_open
zbc_test_init $0 "FINISH ALL zones OK when max_open+1 zones are CLOSED" $*

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
# Create more closed zones than the device is capable of having open (SWR) at a time
zbc_test_close_nr_zones ${seq_zone_type} $(( ${max_open} + 1 ))
if [ $? -ne 0 ]; then
    zbc_test_fail_exit \
	"close_nr_zones ${seq_zone_type} $(( ${max_open} + 1 ))"
fi

# Now try to FINISH ALL closed zones
zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} -1

# Check result
zbc_test_get_sk_ascq
zbc_test_check_no_sk_ascq "(${max_open} + 1) * ${seq_zone_type}"
