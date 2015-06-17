###########################
# Get information functions
###########################

#### zbc_test_get_drive_info
function zbc_test_get_drive_info() {

    sudo ${bin_path}/zbc_test_print_devinfo ${device} >> ${log_file} 2>&1

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

    IFS="$_IFS"

    return 0

}

#### zbc_test_get_zone_info [reporting option]
function zbc_test_get_zone_info() {

    if [ $# -eq 1 ]; then
        ro=${1}
    else
        ro="0"
    fi

    sudo ${bin_path}/zbc_test_report_zones -ro ${ro} ${device} > ${zone_info_file} 2>&1

    return 0

}

###########################
# Preparation functions
###########################
#### zbc_test_open_zones <nr_zones>
function zbc_test_open_nr_zones() {

    open_num=${1}

    declare -i count=0
    for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,0x2,.*,.*,.*,.*"`; do

        _IFS="${IFS}"
        IFS=','
        set -- ${_line}

        zone_type=${3}
        zone_cond=${4}
        start_lba=${5}
        zone_size=${6}
        write_ptr=${7}

        IFS="$_IFS"

        sudo ${bin_path}/zbc_test_open_zone -v ${device} ${start_lba} >> ${log_file} 2>&1
        count=${count}+1

        if [ ${count} -eq $(( ${open_num} )) ]; then
            return 0
        fi

    done

    return 1

}

#### zbc_test_search_vals_from_zone_type <zone_type>
function zbc_test_search_vals_from_zone_type() {

    zone_type=${1}

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

#### zbc_test_search_vals_from_slba <slba>
function zbc_test_search_vals_from_slba() {

    start_lba=${1}

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

        if [ ${start_lba} = ${target_slba} ]; then
            return 0
        fi

    done

    return 1

}

#### zbc_test_search_vals_from_zone_type_and_cond <zone_type> <zone_cond>
function zbc_test_search_vals_from_zone_type_and_cond() {

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

        if [ ${zone_type} = ${target_type} -a ${zone_cond} = ${target_cond} ]; then
            return 0
        fi

    done

    return 1

}

#### zbc_test_search_vals_from_zone_type_and_ignored_cond <zone_type> <ignored_zone_cond>
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

        if [ ${zone_type} = ${target_type} -a ${zone_cond} != ${target_cond} ]; then
            return 0
        fi

    done

    return 1

}


###########################
# Check result functions
###########################
#### zbc_test_get_sk_ascq
function zbc_test_get_sk_ascq() {

    sk=""
    asc=""

    _IFS="${IFS}"
    IFS=','

    sk_line=`cat ${log_file} | grep -F "[SENSE_KEY]"`
    set -- ${sk_line}
    sk=${2}

    asc_line=`cat ${log_file} | grep -F "[ASC_ASCQ]"`
    set -- ${asc_line}
    asc=${2}

    IFS="$_IFS"

    return 0

}

#### zbc_test_print_failed
function zbc_test_print_passed() {

    echo "[TEST][${testname}],Passed"

    return 0

}

function zbc_test_print_failed() {

    echo "[TEST][${testname}],Failed"
    echo "[TEST][${testname}][SENSE_KEY],${sk} expected_sk is ${expected_sk}"
    echo "[TEST][${testname}][ASC_ASCQ],${asc}, expected_asc is ${expected_asc}"

    return 0

}

#### zbc_test_check_asc_std
function zbc_test_check_sk_ascq() {

    if [ ${sk} = ${expected_sk} -a ${asc} = ${expected_asc} ]; then
        zbc_test_print_passed
    else
        zbc_test_print_failed
    fi

    return 0

}

#### zbc_test_check_no_asc
function zbc_test_check_no_sk_ascq() {

    if [ -n ${sk} -a -n ${asc} ]; then
        zbc_test_print_passed
    else
        zbc_test_print_failed
    fi

    return 0

}

