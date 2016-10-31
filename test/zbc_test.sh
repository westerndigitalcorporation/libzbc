#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc.  All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

if [ $# -ne 1 ]; then
  echo "Usage: $0 <target device file>"
  echo "    Ex: $0 /dev/sg3"
  exit 1
fi

device_file=${1}
device_base=`basename ${device_file}`

# Check credentials
if [ $(id -u) -ne 0 ]; then
    echo "Only root can do this."
    exit 1
fi

# Test function
function zbc_run_test()
{
    declare -i run_test_ret=0

    ZBC_TEST_SUB_SCR_PATH=${ZBC_TEST_SCR_PATH}/${1}
    ZBC_TEST_SUB_LOG_PATH=${ZBC_TEST_LOG_PATH}/${device_base}/${1}

    if [ ! -d ${ZBC_TEST_SUB_SCR_PATH} ]; then
        echo "Test script directory ${ZBC_TEST_SUB_SCR_PATH} does not exist"
        exit
    fi

    mkdir -p ${ZBC_TEST_SUB_LOG_PATH}
    cd ${ZBC_TEST_SUB_SCR_PATH}

    for script in *.sh; do
        ./${script} ${device_file} ${ZBC_TEST_BIN_PATH} ${ZBC_TEST_SUB_LOG_PATH}
        script_ret=$?

        if [ ${terminate_mode} -eq 1 -a ${script_ret} -gt 0 ]; then
            run_test_ret=1
            break
        fi
    done

    cd ${ZBC_TEST_DIR}

    return ${run_test_ret}

}

# Set up path
CURRENT_DIR=`pwd`
ZBC_TEST_DIR=$(cd $(dirname $0);pwd)
ZBC_TEST_BIN_PATH=${ZBC_TEST_DIR}/programs
ZBC_TEST_SCR_PATH=${ZBC_TEST_DIR}/scripts
ZBC_TEST_LOG_PATH=${ZBC_TEST_DIR}/log

cd ${ZBC_TEST_DIR}

# Directory check
if [ ! -d ${ZBC_TEST_BIN_PATH} ]; then
    echo "Test program directory ${ZBC_TEST_BIN_PATH} does not exist"
    exit
fi

if [ ! -d ${ZBC_TEST_SCR_PATH} ]; then
    echo "Test script directory ${ZBC_TEST_SCR_PATH} does not exist"
    exit
fi

# Binary check
for bin_name in zbc_test_close_zone zbc_test_finish_zone zbc_test_open_zone zbc_test_print_devinfo zbc_test_read_zone zbc_test_report_zones zbc_test_reset_write_ptr zbc_test_write_zone; do
   bin_path=${ZBC_TEST_BIN_PATH}/${bin_name}
   if [ ! -e ${bin_path} ]; then
       echo "Test program ${bin_name} not found in directory ${ZBC_TEST_BIN_PATH}"
       exit
   fi
done

# Run tests
echo "Executing command completion tests..."
declare -i terminate_mode=1
zbc_run_test 00_command_completion_check ${termination_mode}
zbc_test_ret=$?

if [ ${zbc_test_ret} -gt 0 ]; then
    echo "Exit testing..."
    exit 1
fi

echo "Executing sense key, sense code tests..."
termination_mode=0
zbc_run_test 01_sk_ascq_check ${termination_mode}

echo "Executing zone state machine tests..."
termination_mode=0
zbc_run_test 02_state_machine_check ${termination_mode}

