#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2016, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

# For pretty printing...

red="\e[1;31m"
green="\e[1;32m"
end="\e[m"

# For test script creation:

function zbc_test_init()
{

	if [ $# -ne 5 -a $# -ne 6 ]; then
		echo "Usage$#: $1 <description> <program path> <log path> <section number> <device>"
		exit 1
	fi

	# Store argument
	local _cmd_base=${1##*/}
	local desc="$2"
	bin_path="$3"
	local log_path="$4"
	local section_num="$5"
	device="$6"

	# Case number within section
	case_num="${_cmd_base%.*}"

	if [ -z ${device} ]; then
		# Print description only
		echo "    ${section_num}.${case_num}: ${desc}"
		exit 0
	fi

	echo -n "    ${section_num}.${case_num}: ${desc}..."

	# Test log file
	log_file="${log_path}/${case_num}.log"
	rm -f ${log_file}

	# Zone info file
	zone_info_file="/tmp/${case_num}_zone_info.`basename ${device}`.log"
	rm -f ${zone_info_file}

	# Dump zone info file
	dump_zone_info_file="${log_path}/${case_num}_zone_info.log"
}

function zbc_test_run()
{
	local _cmd="$*"

	echo "" >> ${log_file} 2>&1
	echo "## Executing: ${_cmd}" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1

	${_cmd} >> ${log_file} 2>&1

	return $?
}

# Get information functions

function zbc_check_string()
{
	if [ -z $2 ]; then
		echo "$1"
		exit 1
	fi
}

function zbc_test_get_device_info()
{
	zbc_test_run ${bin_path}/zbc_test_print_devinfo ${device}
	if [ $? != 0 ]; then
		echo "Failed to get device info"
		exit 1
	fi

	local _IFS="${IFS}"
	IFS=$',\n'

	local device_model_line=`cat ${log_file} | grep -F "[DEVICE_MODEL]"`
	set -- ${device_model_line}
	device_model=${2}
	zbc_check_string "Failed to get device model" ${device_model}

	local max_open_line=`cat ${log_file} | grep -F "[MAX_NUM_OF_OPEN_SWRZ]"`
	set -- ${max_open_line}
	max_open=${2}
	zbc_check_string "Failed to get maximum number of open zones" ${max_open}

	local max_lba_line=`cat ${log_file} | grep -F "[MAX_LBA]"`
	set -- ${max_lba_line}
	max_lba=${2}
	zbc_check_string "Failed to get maximum LBA" ${max_lba}

	logical_block_size_line=`cat ${log_file} | grep -F "[LOGICAL_BLOCK_SIZE]"`
	set -- ${logical_block_size_line}
	logical_block_size=${2}
	zbc_check_string "Failed to get logical block size" ${logical_block_size}

	physical_block_size_line=`cat ${log_file} | grep -F "[PHYSICAL_BLOCK_SIZE]"`
	set -- ${physical_block_size_line}
	physical_block_size=${2}
	zbc_check_string "Failed to get physical block size" ${physical_block_size}

	local unrestricted_read_line=`cat ${log_file} | grep -F "[URSWRZ]"`
	set -- ${unrestricted_read_line}
	unrestricted_read=${2}
	zbc_check_string "Failed to get unrestricted read" ${unrestricted_read}

	local last_zone_lba_line=`cat ${log_file} | grep -F "[LAST_ZONE_LBA]"`
	set -- ${last_zone_lba_line}
	last_zone_lba=${2}
	zbc_check_string "Failed to get last zone start LBA" ${last_zone_lba}

	local last_zone_size_line=`cat ${log_file} | grep -F "[LAST_ZONE_SIZE]"`
	set -- ${last_zone_size_line}
	last_zone_size=${2}
	zbc_check_string "Failed to get last zone size" ${last_zone_size}

	IFS="$_IFS"
}

function zbc_test_get_zone_info()
{
	if [ $# -eq 1 ]; then
		local ro="${1}"
	else
		local ro="0"
	fi

	local _cmd="${bin_path}/zbc_test_report_zones -ro ${ro} ${device}"
	echo "" >> ${log_file} 2>&1
	echo "## Executing: ${_cmd} > ${zone_info_file} 2>&1" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1

	${_cmd} > ${zone_info_file} 2>&1

	return 0
}

### [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>

# Issue all zone records to a pipeline
function zbc_zones()
{
	cat ${zone_info_file} | grep -E "\[ZONE_INFO\]"
}

# Remove zones with NON-matching types from the pipeline
# $1 examples:	0x1		match conventional zones, filter others out
#		0x2|0x3		match sequential zones, filter others out
function zbc_zone_filter_in_type()
{
	grep -E "\[ZONE_INFO\],.*,($1),.*,.*,.*,.*"
}

# Remove zones with MATCHING types from the pipeline
# $1 examples:	0x1		filter conventional zones out of the pipeline
#		0x2|0x3		filter sequential zones out of the pipeline
function zbc_zone_filter_out_type()
{
	grep -v -E "\[ZONE_INFO\],.*,($1),.*,.*,.*,.*"
}

# Remove zones with NON-matching conditions from the pipeline
# $1 examples:	0x1		match empty zones, filter others out
#		0x2|0x3		match open zones, filter others out
function zbc_zone_filter_in_cond()
{
	local zone_cond="$1"
	grep -E "\[ZONE_INFO\],.*,.*,($1),.*,.*,.*"
}

# Remove zones with MATCHING conditions from the pipeline
# $1 examples:	0x1		filter empty zones out of pipeline
#		0x2|0x3		filter open zones out of pipeline
function zbc_zone_filter_out_cond()
{
	local zone_cond="$1"
	grep -v -E "\[ZONE_INFO\],.*,.*,($1),.*,.*,.*"
}

# Preparation functions

function zbc_test_count_zones()
{
	nr_zones=`cat ${zone_info_file} | wc -l`
}

function zbc_test_count_conv_zones()
{
	nr_conv_zones=`cat ${zone_info_file} | while IFS=, read a b c d; do echo $c; done | grep -c 0x1`
}

function zbc_test_count_seq_zones()
{
	nr_seq_zones=`cat ${zone_info_file} | while IFS=, read a b c d; do echo $c; done | grep -c 0x2`
}

function zbc_test_open_nr_zones()
{
	local zone_type="0x2"
	local -i count=0

	local -i open_num=${1}

	for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,${zone_type},.*,.*,.*,.*"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		local zone_type=${3}
		local zone_cond=${4}
		local start_lba=${5}
		local zone_size=${6}
		local write_ptr=${7}

		IFS="$_IFS"

		zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${start_lba}
		count=${count}+1

		if [ ${count} -eq $(( ${open_num} )) ]; then
			return 0
		fi

	done

	return 1
}

function zbc_test_search_vals_from_zone_type()
{

	local zone_type="${1}"

	# [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>
	for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,${zone_type},.*,.*,.*,.*"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
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

function zbc_test_search_vals_from_slba()
{

	local start_lba=${1}

	# [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>
	for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,.*,.*,${start_lba},.*,.*"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
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

function zbc_test_search_vals_from_zone_type_and_cond()
{
	local zone_type="${1}"
	local zone_cond="${2}"

	# [ZONE_INFO],<id>,<type>,<cond>,<slba>,<size>,<ptr>
	for _line in `cat ${zone_info_file} | grep "\[ZONE_INFO\],.*,${zone_type},${zone_cond},.*,.*,.*"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
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

function zbc_test_search_vals_from_zone_type_and_ignored_cond()
{

	local zone_type="${1}"
	local zone_cond="${2}"

	for _line in `cat ${zone_info_file} | grep -F "[ZONE_INFO]"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
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

function zbc_test_search_last_zone_vals_from_zone_type()
{

	Found=False
	zone_type=${1}

	for _line in `cat ${zone_info_file} | grep -F "[ZONE_INFO]"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		local local_type=${3}
		local local_cond=${4}
		local local_slba=${5}
		local local_size=${6}
		local local_ptr=${7}

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

function zbc_test_get_sk_ascq()
{
	sk=""
	asc=""

	local _IFS="${IFS}"
	IFS=$',\n'

	local sk_line=`cat ${log_file} | grep -m 1 -F "[SENSE_KEY]"`
	set -- ${sk_line}
	sk=${2}

	local asc_line=`cat ${log_file} | grep -m 1 -F "[ASC_ASCQ]"`
	set -- ${asc_line}
	asc=${2}

	IFS="$_IFS"
}

function zbc_test_print_res()
{
	local width=`tput cols`

	width=$(($width-9))
	if [ ${width} -gt 90 ]; then
		width=90
	fi

	echo "" >> ${log_file} 2>&1
	echo "TESTRESULT==$2" >> ${log_file} 2>&1
	echo -e "\r\e[${width}C[$1$2${end}]"
}

function zbc_test_print_passed()
{
	zbc_test_print_res "${green}" "Passed"
}

function zbc_test_print_not_applicable()
{
	zbc_test_print_res "" " N/A  "
	exit
}

function zbc_test_print_failed_sk()
{
	zbc_test_print_res "${red}" "Failed"

	echo "=> Expected ${expected_sk} / ${expected_asc}, Got ${sk} / ${asc}" >> ${log_file} 2>&1
	echo "            => Expected ${expected_sk} / ${expected_asc}"
	echo "               Got ${sk} / ${asc}"
}

function zbc_test_check_sk_ascq()
{
	if [ "${sk}" = "${expected_sk}" -a "${asc}" = "${expected_asc}" ]; then
		zbc_test_print_passed
	else
		zbc_test_print_failed_sk
	fi
}

function zbc_test_check_no_sk_ascq()
{
	if [ -z "${sk}" -a -z "${asc}" ]; then
		zbc_test_print_passed
	else
		zbc_test_print_failed_sk
	fi
}

function zbc_test_print_failed_zc()
{
	zbc_test_print_res "${red}" "Failed"

	echo "=> Expected zone condition ${expected_cond}, Got ${target_cond}" >> ${log_file} 2>&1
	echo "            => Expected zone condition ${expected_cond}"
	echo "               Got ${target_cond}"
}

function zbc_test_check_zone_cond()
{
	if [ ${target_cond} == ${expected_cond} ]; then
		zbc_test_check_no_sk_ascq
	else
		zbc_test_print_failed_zc
	fi
}

function zbc_test_check_zone_cond_sk_ascq()
{
	if [ ${target_cond} == ${expected_cond} ]; then
		zbc_test_check_sk_ascq
	else
		zbc_test_print_failed_zc
	fi
}

function zbc_test_dump_zone_info()
{
	zbc_report_zones ${device} > ${dump_zone_info_file}
}

function zbc_test_check_failed()
{

	failed=`cat ${log_file} | grep -m 1 "^Failed"`

	if [ "Failed" = "${failed}" ]; then
		zbc_test_dump_zone_info
		return 1
	fi

	return 0
}
