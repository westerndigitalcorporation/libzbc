#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.

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

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Get zone information
zbc_test_get_zone_info

# if max_open == -1 then it is "not reported"
if [ ${max_open} -eq -1 ]; then
    zbc_test_print_not_applicable
fi

# Open zones
# Get the number of available sequential zones
nr_avail_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${zone_type}" \
			      | zbc_zone_filter_in_cond "0x1" | wc -l`

if [ ${max_open} -ge ${nr_avail_seq_zones} ]; then
    zbc_test_print_not_applicable "Not enough (${nr_avail_seq_zones}) available zones" \
				  "of type ${zone_type} to exceed max_open (${max_open})"
fi

zbc_test_open_nr_zones ${max_open}

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_lba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq

# Post process
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} -1
rm -f ${zone_info_file}

