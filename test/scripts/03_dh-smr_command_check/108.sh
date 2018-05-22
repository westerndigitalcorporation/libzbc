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

zbc_test_init $0 "ZONE ACTIVATE(32): attempt to deactivate NON-EMPTY non-first zone in non-first domain (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="0x4100"	# CBI | ZNRESET

zbc_test_get_device_info

if [ ${seq_req_zone} -ne 0 ]; then
    smr_type="seq"
elif [ ${seq_pref_zone} -ne 0 ]; then
    smr_type="seqp"
else
    zbc_test_print_not_applicable "Neither SWR nor SWP zones are supported by the device"
fi

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${wpc_zone} -ne 0 ]; then
    cmr_type="wpc"
else
    zbc_test_print_not_applicable "Neither conventional nor WPC zones are supported by the device"
fi

zbc_test_get_zone_info
zbc_test_get_cvt_domain_info

# Find a CMR domain that is convertible to SMR
# Assume there are two in a row
zbc_test_search_domain_by_type_and_cvt "0x1|0x4" "seq"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No domain is currently CMR and convertible to SMR"
fi

# Calculate number of zones in two domains starting at ${domain_num}
zbc_test_calc_nr_domain_zones ${domain_num} 2		# into ${nr_conv_zones}

# Record ACTIVATE start LBA and number of zones for both directions
conv_lba=${domain_conv_start}
conv_nz=${nr_conv_zones}
seq_lba=${domain_seq_start}
seq_nz=${nr_seq_zones}

# Lookup info on the second domain			# into ${domain_*}
zbc_test_search_cvt_domain_by_number $(( ${domain_num} + 1 ))

# Lookup info on the second domain's first SMR zone
zbc_test_search_vals_from_slba ${domain_seq_start}	# into ${target_*}

# Calculate the start LBA of the second domain's second zone
write_zlba=$(( ${target_slba} + ${target_size} ))
expected_err_cbf="${write_zlba}"

# Start testing
# Convert the two domains to SMR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${conv_lba} ${conv_nz} ${smr_type}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Failed to convert domain to SMR type ${smr_type}"

# Write an LBA in the second zone of the second domain to make it NON-EMPTY
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} ${write_zlba} 1
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed"

# Now try to convert the domains from SMR back to CMR
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${seq_lba} ${seq_nz} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
if [ "${smr_type}" = "seqp" ]; then
    #XXX Arguably a SEQP zone must be empty to deactivate, but the emulator allows non-empty for now
    zbc_test_check_no_sk_ascq
else
    zbc_test_check_err
fi

# Check failed
zbc_test_check_failed

# Post-processing -- put the domain back the way we found it
zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${write_zlba}
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${seq_lba} ${seq_nz} ${cmr_type}

# Post process
rm -f ${zone_info_file}
