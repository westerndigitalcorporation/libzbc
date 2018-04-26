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

zbc_test_init $0 "ZONE ACTIVATE(32) LBA crossing out of range (zone addressing)" $*

# Set expected error code
# ZA-r4 5.y.3.1 ZONE ID plus NUMBER OF ZONES is out of range
expected_sk="Unknown-sense-key 0x00"
expected_asc="Unknown-additional-sense-code-qualifier 0x00"
expected_err_za="0x0200"	# BADNRZ
expected_err_cbf="0"

# Get information
zbc_test_get_device_info
zbc_test_get_zone_info
zbc_test_get_cvt_domain_info

# Select last domain
zbc_test_count_cvt_domains		# into nr_domains
zbc_test_search_cvt_domain_by_number $(( ${nr_domains} - 1 ))

# Use double the size of the last domain when trying ACTIVATE across EOM

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${domain_seq_start} $(( 2 * ${domain_seq_len} )) "conv"

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Check failed
zbc_test_check_failed
