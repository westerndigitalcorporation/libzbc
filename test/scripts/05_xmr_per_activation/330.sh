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

zbc_test_init $0 "CLOSE an INACTIVE zone (type=${test_zone_type:-${ZT_SEQ}})" $*

# Get drive information
zbc_test_get_device_info

# Search target zone
zbc_test_search_zone_cond_or_NA ${ZC_INACTIVE}

if [[ ${target_type} == ${ZT_CONV} ]]; then
	expected_sk="Illegal-request"
	expected_asc="Invalid-field-in-cdb"
else
	expected_sk="Data-protect"
	expected_asc="Zone-is-inactive"
fi

# Start testing
zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_slba}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq "zone_type=${target_type}"
