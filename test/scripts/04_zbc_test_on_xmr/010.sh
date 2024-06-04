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

zbc_test_init $0 "Run ZBC test on initial device configuration" $*

ZBC_TEST_LOG_PATH_BASE=${2}/allcmr

zbc_test_get_device_info

# Let us assume that all the available Write Pointer zones are EMPTY...
zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1

# Pass the batch_mode flag through to the run we invoke below
arg_b=""
if [ ${ZBC_TEST_BATCH_MODE} -ne 0 ] ; then
    arg_b="-b"
fi

arg_a=""
if [ ${ZBC_TEST_FORCE_ATA} ]; then
    arg_a="-a"
fi

arg_w=""
if [ ${ZBC_ACCEPT_ANY_FAIL} ]; then
    arg_w="-w"
fi

arg_l=""
if [ ${RUN_ACTIVATIONS_ONLY} ]; then
    arg_l="-l"
fi
arg_x=""
if [ ${ZBC_RUN_EXTENDED_TESTS} ]; then
    arg_x="-x"
fi

# Specify post-processing to occur when script exits
zbc_test_case_on_exit zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1

# Start ZBC test
zbc_test_meta_run ./zbc_test.sh ${arg_a} ${arg_b} ${arg_w} ${arg_x} ${arg_l} -n ${eexec_list} ${cskip_list} ${device}
if [ $? -ne 0 ]; then
    sk="04.010 fail -- log path ${ZBC_TEST_LOG_PATH_BASE}"
    asc="child test of 04.010 failed $?"
fi

# Check result
zbc_test_check_no_sk_ascq
