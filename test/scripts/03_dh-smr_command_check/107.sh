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
#

. scripts/zbc_test_lib.sh

zbc_test_init $0 "ZONE ACTIVATE(32) attempt to deactivate NON-EMPTY zone (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4100"	# CBI | ZNRESET

# Get drive information
zbc_test_get_device_info

# Get conversion domain information
zbc_test_get_cvt_domain_info

# Find a conventional domain that is convertable to SWR
zbc_test_search_domain_by_type_and_cvt "1" "seq"
if [ $? -ne 0 ]; then
	zbc_test_print_not_applicable "No domain is currently conventional and convertible to SWR"
fi
expected_err_cbf="${domain_seq_start}"

# Start testing
# Convert the domain to SWR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_conv_start} ${domain_conv_len} "seq"

# Make the first zone of the domain non-empty
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} ${domain_seq_start} 1

# Now try to convert the domain from SWR back to conventional
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_seq_start} ${domain_seq_len} "conv"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed

# Post-processing -- put the domain back the way we found it
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${domain_seq_start}
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_seq_start} ${domain_seq_len} "conv"
