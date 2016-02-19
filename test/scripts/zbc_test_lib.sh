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

# For pretty printing...

red="\e[1;33m"
green="\e[1;32m"
end="\e[m"

# For test script creation:

function zbc_test_init() {


    if [ $# -ne 3 -a $# -ne 4 ]; then
        echo "Usage: $1 <target device> <test program path> [test log file path]"
        echo "    target_device:      path to the target device file (e.g. /dev/sg3)"
        echo "    test program path:  Path to the directory containing the compiled test programs"
        echo "    test log file path: Path to the directory where to output the test log file"
        echo "                        (the default is the current working directory)"
        exit 1
    fi

    # Store argument
    device=$2
    bin_path=$3

    # Test name
    local _cmd_base=${1##*/}
    test_name="${_cmd_base%.*}"

    # Log file
    if [ $# -eq 4 ]; then
        log_path="$4"
    else
        log_path="`pwd`"
    fi
    log_file="${log_path}/${test_name}.log"
    rm -f ${log_file}

    # Zone info file
    zone_info_file="/tmp/${test_name}_zone_info.log"
    rm -f ${zone_info_file}

    # Dump zone info file
    dump_zone_info_file="${log_path}/${test_name}_zone_info.log"

}

function zbc_test_info() {

    echo -n "    ${test_name}: $1 "

}

function zbc_test_run() {

    local _cmd="$*"

    echo "" >> ${log_file} 2>&1
    echo "## Executing: ${_cmd}" >> ${log_file} 2>&1
    echo "" >> ${log_file} 2>&1

    ${_cmd} >> ${log_file} 2>&1

    return $?

}

# Get information functions

function zbc_test_get_drive_info() {

    zbc_test_run ${bin_path}/zbc_test_print_devinfo ${device}

    _IFS="${IFS}"
    IFS=','

    max_open_line=`cat ${log_file} | grep -F "[MAX_NUM_OF_OPEN_SWRZ]"`
    set -- ${max_open_line}
    max_open=${2}

    max_lba_line=`cat ${log_file} | grep -F "[MAX_LBA]"`
    set -- ${max_lba_line}
    max_lba=${2}

    unrestricted_read_line=`cat ${log_file} | grep -F "[URSWRZ]"`
    set -- ${unrestricted_read_line}
    unrestricted_read=${2}

    last_zone_lba_line=`cat ${log_file} | grep -F "[LAST_ZONE_LBA]"`
    set -- ${last_zone_lba_line}
    last_zone_lba=${2}

    last_zone_size_line=`cat ${log_file} | grep -F "[LAST_ZONE_SIZE]"`
    set -- ${last_zone_size_line}
    last_zone_size=${2}

    IFS="$_IFS"

    return 0

}

function zbc_test_get_zone_info() {

    if [ $# -eq 1 ]; then
        ro=${1}
    else
        ro="0"
    fi

    local _cmd="${bin_path}/zbc_test_report_zones -ro ${ro} ${device}"
    echo "" >> ${log_file} 2>&1
    echo "## Executing: ${_cmd} > ${zone_info_file} 2>&1" >> ${log_file} 2>&1
    echo "" >> ${log_file} 2>&1

    ${_cmd} > ${zone_info_file} 2>&1

    return 0

}

# Preparation functions

function zbc_test_open_nr_zones() {

    open_num=${1}
    local zone_type="0x2"

    declare -i count=0
    for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,${zone_type},.*,.*,.*,.*"`; do

        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        zone_type=${3}
        zone_cond=${4}
        start_lba=${5}
        zone_size=${6}
        write_ptr=${7}

        IFS="$_IFS"

        zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${start_lba}
        count=${count}+1

        if [ ${count} -eq $(( ${open_num} )) ]; then
            return 0
        fi

    done

    return 1

}

function zbc_test_search_vals_from_zone_type() {

    zone_type=${1}

    # [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>
    for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,${zone_type},.*,.*,.*,.*"`; do

        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        target_type=${3}
        target_cond=${4}
        target_slba=${5}
        target_size=${6}
        target_ptr=${7}

        IFS="$_IFS"

        return 0

    done

    return 1

}

function zbc_test_search_vals_from_slba() {

    start_lba=${1}

    # [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>
    for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,.*,.*,${start_lba},.*,.*"`; do
        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        target_type=${3}
        target_cond=${4}
        target_slba=${5}
        target_size=${6}
        target_ptr=${7}

        IFS="$_IFS"

        return 0

    done

    return 1

}

function zbc_test_search_vals_from_zone_type_and_cond() {

    zone_type=${1}
    zone_cond=${2}

    # [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>
    for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,${zone_type},${zone_cond},.*,.*,.*"`; do

        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        target_type=${3}
        target_cond=${4}
        target_slba=${5}
        target_size=${6}
        target_ptr=${7}

        IFS="$_IFS"

        return 0

    done

    return 1

}

function zbc_test_search_vals_from_zone_type_and_ignored_cond() {

    zone_type=${1}
    zone_cond=${2}

    for _line in `cat ${zone_info_file} | grep -F "[ZONE_INFO]"`; do

        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        target_type=${3}
        target_cond=${4}
        target_slba=${5}
        target_size=${6}
        target_ptr=${7}

        IFS="$_IFS"

        if [ "${zone_type}" = "${target_type}" -a "${zone_cond}" != "${target_cond}" ]; then
            return 0
        fi

    done

    return 1

}

function zbc_test_search_last_zone_vals_from_zone_type() {

    Found=False
    zone_type=${1}

    for _line in `cat ${zone_info_file} | grep -F "[ZONE_INFO]"`; do

        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        local_type=${3}
        local_cond=${4}
        local_slba=${5}
        local_size=${6}
        local_ptr=${7}

        IFS="$_IFS"

        if [ "${zone_type}" = "${local_type}" ]; then
            Found=True
            target_type=${local_type}
            target_cond=${local_cond}
            target_slba=${local_slba}
            target_size=${local_size}
            target_ptr=${local_ptr}
        fi

    done

    if [ ${Found} = "False" ]; then

        return 1

    fi

    return 0

}

# Check result functions

function zbc_test_get_sk_ascq() {

    sk=""
    asc=""

    _IFS="${IFS}"
    IFS=','

    sk_line=`cat ${log_file} | grep -m 1 -F "[SENSE_KEY]"`
    set -- ${sk_line}
    sk=${2}

    asc_line=`cat ${log_file} | grep -m 1 -F "[ASC_ASCQ]"`
    set -- ${asc_line}
    asc=${2}

    IFS="$_IFS"

    return 0

}

function zbc_test_print_passed() {

    echo "" >> ${log_file} 2>&1
    echo "Passed" >> ${log_file} 2>&1

    echo -e "\r\e[120C[${green}Passed${end}]"

    return 0

}

function zbc_test_print_failed_sk() {

    echo "" >> ${log_file} 2>&1
    echo "Failed" >> ${log_file} 2>&1
    echo "=> Expected ${expected_sk} / ${expected_asc}, Got ${sk} / ${asc}" >> ${log_file} 2>&1

    echo -e "\r\e[120C[${red}Failed${end}]"
    echo "        => Expected ${expected_sk} / ${expected_asc}"
    echo "           Got ${sk} / ${asc}"

    return 0

}

function zbc_test_check_sk_ascq() {

    if [ "${sk}" = "${expected_sk}" -a "${asc}" = "${expected_asc}" ]; then
        zbc_test_print_passed
    else
        zbc_test_print_failed_sk
    fi

    return 0

}

function zbc_test_check_no_sk_ascq() {

    if [ -z "${sk}" -a -z "${asc}" ]; then
        zbc_test_print_passed
    else
        zbc_test_print_failed_sk
    fi

    return 0

}

function zbc_test_print_failed_zc() {

    echo "" >> ${log_file} 2>&1
    echo "Failed" >> ${log_file} 2>&1
    echo "=> Expected zone_condition ${expected_cond}, Got ${target_cond}" >> ${log_file} 2>&1

    echo -e "\r\e[120C[${red}Failed${end}]"
    echo "        => Expected zone_condition ${expected_cond}"
    echo "           Got ${target_cond}"

    return 0

}

function zbc_test_print_not_applicable() {

    echo "" >> ${log_file} 2>&1
    echo "N/A" >> ${log_file} 2>&1

    echo -e "\r\e[120C[${green} N/A  ${end}]"

    return 0

}

function zbc_test_check_zone_cond() {

    if [ ${target_cond} == ${expected_cond} ]; then
        zbc_test_check_no_sk_ascq
    else
        zbc_test_print_failed_zc
    fi

    return 0

}

function zbc_test_check_zone_cond_sk_ascq() {

    if [ ${target_cond} == ${expected_cond} ]; then
        zbc_test_check_sk_ascq
    else
        zbc_test_print_failed_zc
    fi

    return 0

}

function zbc_test_dump_zone_info() {

    zbc_report_zones ${device} > ${dump_zone_info_file}

    return 0
}

function zbc_test_check_failed() {

    failed=`cat ${log_file} | grep -m 1 "^Failed"`

    if [ "Failed" = "${failed}" ]; then
        zbc_test_dump_zone_info
        return 1
    fi

    return 0

}

