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

zbc_test_init $0 "ZONE ACTIVATE(32): deactivate Implicitly-OPEN non-first realm" $*

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
    zbc_test_print_not_applicable "No sequential zones are supported by the device"
fi

if [ ${conv_zone} -ne 0 ]; then
    cmr_type="conv"
elif [ ${sobr_zone} -ne 0 ]; then
    cmr_type="sobr"
else
    zbc_test_print_not_applicable "No non-sequential zones are supported by the device"
fi

zbc_test_get_zone_info
zbc_test_get_zone_realm_info

# Find a conventional realm that can be activated as sequential
# Assume there are two in a row
zbc_test_search_realm_by_type_and_actv "${ZT_NON_SEQ}" "seq" "NOFAULTY"
if [ $? -ne 0 ]; then
    zbc_test_print_not_applicable "No realm is currently conventional and can be activated as sequential"
fi
# Record ACTIVATE start LBA for both directions
conv_lba=$(zbc_realm_cmr_start)
seq_lba=$(zbc_realm_smr_start)

# Calculate number of zones in two realms starting at ${realm_num}
zbc_test_calc_nr_realm_zones ${realm_num} 2		# into ${nr_conv_zones}

# Record ACTIVATE number of zones for both directions
conv_nz=${nr_conv_zones}
seq_nz=${nr_seq_zones}

# Lookup info on the second realm			# into ${realm_*}
zbc_test_search_zone_realm_by_number $(( ${realm_num} + 1 ))

# Lookup info on the second realm's first sequential zone
zbc_test_get_target_zone_from_slba $(zbc_realm_smr_start)	# into ${target_*}

# Calculate the start LBA of the second realm's second zone
write_zlba=$(( ${target_slba} + ${target_size} ))
expected_err_cbf="${write_zlba}"

# Start testing
if [ cmr_type = "wpc" ]; then
    # Make sure the deactivating zones are EMPTY
    zbc_test_run ${bin_path}/zbc_test_reset_zone -v -32 -z ${device} -1
fi

# Activate the two realms as sequential
zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${conv_lba} ${conv_nz} ${smr_type}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Failed to activate realm to sequential type ${smr_type}"

# Write an LBA in the second zone of the second realm to make it NON-EMPTY
zbc_test_run ${bin_path}/zbc_test_write_zone ${device} ${write_zlba} ${lblk_per_pblk}
zbc_test_get_sk_ascq
zbc_test_fail_if_sk_ascq "Initial write failed at $(zbc_realm_smr_start) zone_type=${smr_type}"

# Now try to activate the sequential realms back as conventional
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

# Post-processing -- put the realm back the way we found it
if [ "${smr_type}" != "seqp" ]; then
    # Zone did not deactivate -- reset and deactivate it
    zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${write_zlba}
    zbc_test_run ${bin_path}/zbc_test_zone_activate -v -32 -z ${device} ${seq_lba} ${seq_nz} ${cmr_type}
fi

# Post process
rm -f ${zone_info_file}
