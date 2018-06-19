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

zbc_test_init $0 "ZONE ACTIVATE(32): LBA range crosses domain boundary (zone addr)" $*

# Set expected error code
expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"

# Get information
zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "Neither SWR nor SWP zones are supported by the device"
fi

zbc_test_get_zone_info
zbc_test_get_cvt_domain_info

zbc_test_count_cvt_domains		# into nr_domains
zbc_test_search_cvt_domain_by_number $(( ${nr_domains} - 1 ))

target_lba=${domain_conv_start}
target_nzone=$(( ${domain_conv_len} + ${domain_seq_len} ))

# Start testing
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${target_lba} ${target_nzone} ${smr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err

# Post process
zbc_test_check_failed
rm -f ${zone_info_file}
