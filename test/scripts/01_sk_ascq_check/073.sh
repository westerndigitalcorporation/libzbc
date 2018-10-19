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

zbc_test_init $0 "WRITE full zone (type=${test_zone_type:-${ZT_SEQ}})" $*


# Get drive information
zbc_test_get_device_info

# Search target zone
zbc_test_search_zone_cond_or_NA "${ZC_EMPTY}|${ZC_NOT_WP}"

if [ ${target_type} = ${ZT_SWR} ]; then
	# Expected error code for sequential write required zones
	expected_sk="Illegal-request"
	expected_asc="Invalid-field-in-cdb"
else
	expected_sk=""
	expected_asc=""
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} ${target_slba}

# Start testing
if [ ${target_type} != ${ZT_SOBR} ]; then
	zbc_test_run ${bin_path}/zbc_test_finish_zone -v ${device} ${target_slba}
else
	zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_ptr} ${lblk_per_pblk}
fi
zbc_test_fail_exit_if_sk_ascq "FINISH failed, zone_type=${target_type}"

zbc_test_run ${bin_path}/zbc_test_write_zone -v -n 1 ${device} ${target_slba} ${lblk_per_pblk}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
