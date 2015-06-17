#!/bin/bash

if [ $# -ne 1 ]; then
  echo "[usage] $0 <target_device>"
  echo "    target_device          : device file. e.g. /dev/sg3"
  exit 1
fi

# Store argument
device_file=${1}

# Test function
function zbc_run_test()
{

    ZBC_TEST_SUB_SCR_PATH=${ZBC_TEST_SCR_PATH}/${1}
    ZBC_TEST_SUB_LOG_PATH=${ZBC_TEST_LOG_PATH}/${1}

    if [ ! -d ${ZBC_TEST_SUB_SCR_PATH} ]; then
        echo "[TEST][ERROR],Target directory [${ZBC_TEST_SUB_SCR_PATH}] doesn't exist"
        exit
    fi
   
    mkdir -p ${ZBC_TEST_SUB_LOG_PATH}
    cd ${ZBC_TEST_SUB_SCR_PATH}

    for script in *.sh; do
        ./${script} ${device_file} ${ZBC_TEST_BIN_PATH} ${ZBC_TEST_SUB_LOG_PATH}
    done

    cd ${ZBC_TEST_DIR}

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
    echo "[TEST][ERROR],Directory [${ZBC_TEST_BIN_PATH}] doesn't exist"
    exit
fi

if [ ! -d ${ZBC_TEST_SCR_PATH} ]; then
    echo "[TEST][ERROR],Directory [${ZBC_TEST_SCR_PATH}] doesn't exist"
    exit
fi

# Binary check
for bin_name in zbc_test_close_zone zbc_test_finish_zone zbc_test_open_zone zbc_test_print_devinfo zbc_test_read_zone zbc_test_report_zones zbc_test_reset_write_ptr zbc_test_write_zone; do
   bin_path=${ZBC_TEST_BIN_PATH}/${bin_name}
   if [ ! -e ${bin_path} ]; then
       echo "[TEST][ERROR],[${bin_name}] is not found in directory [${ZBC_TEST_BIN_PATH}]"
       exit
   fi
done

# Run test
echo "[TEST] Start testing 01_command_check"
zbc_run_test 01_command_check

echo "[TEST] Start testing 02_state_machine_check"
zbc_run_test 02_state_machine_check

