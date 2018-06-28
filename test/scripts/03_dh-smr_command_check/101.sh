#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "ZONE ACTIVATE(32): LBA starting out of range (zone addressing)" $*

# Set expected error code
# ZA-r4 5.y.3.1 ZONE ID does not specify the lowest LBA of a Zone
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get information
zbc_test_get_device_info

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${sobr_zone} -ne 0 ]; then
    cmr_type="sobr"
else
    zbc_test_print_not_applicable "No non-sequential zones are supported by the device"
fi

zbc_test_get_zone_info
zbc_test_get_zone_realm_info

# Select last realm
zbc_test_count_zone_realms		# into nr_realms
zbc_test_search_zone_realm_by_number $(( ${nr_realms} - 1 ))

# Use the size of the last realm when trying ACTIVATE at End of Medium

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} $(( ${max_lba} + 1 )) $(zbc_realm_cmr_len) ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

# Post process
rm -f ${zone_info_file}
