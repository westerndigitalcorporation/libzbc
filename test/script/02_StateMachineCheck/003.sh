#!/bin/bash

if [ $# -ne 1 ]; then
  echo "[usage] $0 <target_device>"
  echo "    target_device          : device file. e.g. /dev/sg3"
  exit 1
fi

testbase=${0##*/}
testname=${testbase%.*}

device=${1}
nr_test=${testname}

expected_sk=Illegal-request
expected_asc=Invalid-field-in-cdb


# Delete old log
rm -Rf ${nr_test}.log
rm -Rf ${nr_test}_zone_info.log

# Test print
echo "[TEST][${nr_test}][CZ][FINISH_ZONE][INVALID_FIELD_IN_CDB] start"


######################## Get drive info ######################## 
sudo zbc_test_print_devinfo ${device} >> ${nr_test}.log 2>&1

# Get zbc specific info
_IFS="${IFS}"
IFS=','

max_open_line=`cat ${nr_test}.log | grep -F "[MAX_NUM_OF_OPEN_SWRZ]"`
set -- ${max_open_line}
max_open=${2}

max_lba_line=`cat ${nr_test}.log | grep -F "[MAX_LBA]"`
set -- ${max_lba_line}
max_lba=${2}

unrestricted_read_line=`cat ${nr_test}.log | grep -F "[URSWRZ]"`
set -- ${unrestricted_read_line}
unrestricted_read=${2}

IFS="$_IFS"


######################## Get zone info ######################## 
# Get zone info
sudo zbc_test_report_zones ${device} >> ${nr_test}_zone_info.log 2>&1

######################## Determin target zone ######################## 
_IFS="${IFS}"
IFS=','

cat ${nr_test}_zone_info.log | grep -F "[ZONE_INFO]" |\
while read _line; do
    set -- ${_line}

    zone_type=${3}
    start_lba=${5}

    if [ ${zone_type} = "0x1" ]; then
        target_lba=$(( ${start_lba} ))
        echo ${target_lba} > test_zone_lba.txt
        break;
    fi

done

IFS="$_IFS"

######################## Start Testing ######################## 
# Execution command
if [ -e test_zone_lba.txt ]; then
    test_zone_lba=`cat test_zone_lba.txt`
    rm test_zone_lba.txt
else
    echo "[TEST][${nr_test}],Failed"
    echo "[TEST][${nr_test}],can't open test_zone_lba.txt"
    exit 1
fi

sudo zbc_test_finish_zone -v ${device} ${test_zone_lba} >> ${nr_test}.log 2>&1

# Get SenseKey, ASC/ASCQ
_IFS="${IFS}"
IFS=','

sk_line=`cat ${nr_test}.log | grep -F "[SENSE_KEY]"`
set -- ${sk_line}
sk=${2}

asc_line=`cat ${nr_test}.log | grep -F "[ASC_ASCQ]"`
set -- ${asc_line}
asc=${2}

IFS="$_IFS"


######################## Check result ######################## 
# Check result
if [ ${sk} = ${expected_sk} -a ${asc} = ${expected_asc} ]; then
    echo "[TEST][${nr_test}],Passed"
else
    echo "[TEST][${nr_test}],Failed"
    echo "[TEST][${nr_test}][SENSE_KEY],${sk} instead of ${expected_sk}"
    echo "[TEST][${nr_test}][ASC_ASCQ],${asc} instead of ${expected_asc}"
fi


######################## Reset all zones ######################## 
rm -Rf ${nr_test}_zone_info.log

