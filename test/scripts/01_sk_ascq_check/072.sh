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
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "WRITE insufficient zone resources" $*

# Set expected error code
expected_sk="Data-protect"
expected_asc="Insufficient-zone-resources"

# Get drive information
zbc_test_get_device_info

if [ ${device_model} = "Host-aware" ]; then
    zone_type="0x3"
else
    zone_type="0x2"
fi

# Let us assume that all the available sequential zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Get zone information
zbc_test_get_zone_info

# Check the number of inactive zones
zbc_test_count_inactive_zones

# Check number of sequential zones
zbc_test_count_seq_zones

#XXX Shouldn't this be subtracting the number of inactive SEQUENTIAL zones?
if [ ${max_open} -ge $((${nr_seq_zones} - ${nr_inactive_zones})) ]; then
    zbc_test_print_not_applicable "Not enough active zones: (max_open=${max_open}) >= (nr_seq_zones=${nr_seq_zones}) - (nr_inactive_zones=${nr_inactive_zones})"
fi

# if max_open == -1 then it is "not reported"
if [ ${max_open} -eq -1 ]; then
    zbc_test_print_not_applicable "max_open not reported"
fi

# Open zones
zbc_test_open_nr_zones ${max_open}

# Get zone information
zbc_test_get_zone_info

# Search target LBA
zbc_test_get_target_zone_from_type_and_cond ${zone_type} "0x1"
target_lba=${target_slba}

# Start testing
zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_lba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq

if [ ${device_model} = "Host-aware" ]; then
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_sk_ascq
fi

# Post process
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

rm -f ${zone_info_file}

