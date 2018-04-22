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

function zbc_test_lib_init()
{
	# Zone types with various attributes
	declare -rg ZT_CONV="0x1"			# Conventional zone
	declare -rg ZT_SWR="0x2"			# Sequential Write Required zone
	declare -rg ZT_SWP="0x3"			# Sequential Write Preferred zone

	# Example Usage:  if [[ ${target_type} == @(${ZT_NON_SEQ}) ]]; then...
	#                 if [[ ${target_type} != @(${ZT_WP}) ]]; then...

	declare -rg ZT_NON_SEQ="${ZT_CONV}"		# CMR
	declare -rg ZT_SEQ="${ZT_SWR}|${ZT_SWP}"	# SMR
	declare -rg ZT_WP="${ZT_SEQ}"			# Write Pointer zone

	declare -rg ZT_DISALLOW_WRITE_GT_WP="0x2"	# Write starting above WP disallowed
	declare -rg ZT_DISALLOW_WRITE_LT_WP="0x2"	# Write starting below WP disallowed
	declare -rg ZT_DISALLOW_WRITE_XZONE="0x2"	# Write across zone boundary disallowed
	declare -rg ZT_DISALLOW_WRITE_FULL="0x2"	# Write FULL zone disallowed
	declare -rg ZT_REQUIRE_WRITE_PHYSALIGN="0x2"	# Write ending >= WP must be physical-block-aligned

	declare -rg ZT_RESTRICT_READ_XZONE="0x2"	# Read across zone boundary disallowed when !URSWRZ
	declare -rg ZT_RESTRICT_READ_GE_WP="0x2"	# Read ending above WP disallowed when !URSWRZ

	declare -rg ZT_W_OZR="0x2"			# Participates in Open Zone Resources protocol

	# Zone conditions
	declare -rg ZC_NOT_WP="0x0"			# NOT_WRITE_POINTER zone condition
	declare -rg ZC_EMPTY="0x1"			# EMPTY zone condition
	declare -rg ZC_IOPEN="0x2"			# IMPLICITLY OPEN zone condition
	declare -rg ZC_EOPEN="0x3"			# EXPLICITLY OPEN zone condition
	declare -rg ZC_OPEN="${ZC_IOPEN}|${ZC_EOPEN}"	# Either OPEN zone condition
	declare -rg ZC_CLOSED="0x4"			# CLOSED zone condition
	declare -rg ZC_FULL="0xe"			# FULL zone condition
	declare -rg ZC_NON_FULL="0x0|0x1|0x2|0x3|0x4"	# Non-FULL available zone conditions
	declare -rg ZC_AVAIL="${ZC_NON_FULL}|${ZC_FULL}" # available zone conditions
}

if [ -z "${ZBC_TEST_LIB_INIT}" ]; then
	zbc_test_lib_init
	ZBC_TEST_LIB_INIT=1
fi

# Trim leading zeros off of result strings so expr doesn't think they are octal
function _trim()
{
	echo $1 | sed -e "s/^00*\(.\)/\1/"
}

# For test script creation:
function zbc_test_init()
{

	if [ $# -ne 5 -a $# -ne 6 ]; then
		echo "Usage: $1 <description> <program path> <log path> <section number> <device>"
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

	# Conversion domain info file
	cvt_domain_info_file="/tmp/${case_num}_cvt_domain_info.`basename ${device}`.log"
	rm -f ${cvt_domain_info_file}

	# Dump zone info file
	dump_zone_info_file="${log_path}/${case_num}_zone_info.log"

	# Dump conversion domain info file
	dump_cvt_domain_info_file="${log_path}/${case_num}_cvt_domain_info.log"
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

function zbc_test_meta_run()
{
	local _cmd="$*"

	echo "" >> ${log_file} 2>&1
	echo "## Executing: ${_cmd}" >> ${log_file} 2>&1
	echo "" 2>&1 | tee ${log_file} 2>&1

	${_cmd} 2>&1 | tee ${log_file} 2>&1
	ret=${PIPESTATUS[0]}

	return ${ret}
}

# Get information functions

function zbc_check_string()
{
	if [ -z "$2" ]; then
		echo "$1"
		exit 1
	fi
}

function zbc_test_get_device_info()
{
	zbc_test_run ${bin_path}/zbc_test_print_devinfo ${device}
	if [ $? -ne 0 ]; then
		echo "Failed to get device info for ${device}"
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

	lblk_per_pblk=$((physical_block_size/logical_block_size))

	local unrestricted_read_line=`cat ${log_file} | grep -F "[URSWRZ]"`
	set -- ${unrestricted_read_line}
	unrestricted_read=${2}
	zbc_check_string "Failed to get unrestricted read" ${unrestricted_read}

	zone_activation_device_line=`cat ${log_file} | grep -F "[ZONE_ACTIVATION_DEVICE]"`
	set -- ${zone_activation_device_line}
	zone_activation_device=${2}
	zbc_check_string "Failed to get Zone Activation device support" ${zone_activation_device}

	last_zone_lba_line=`cat ${log_file} | grep -F "[LAST_ZONE_LBA]"`
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

	${_cmd} > ${zone_info_file} 2>> ${log_file}

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
	nr_zones=`cat ${zone_info_file} | grep "\[ZONE_INFO\]" | wc -l`
}

function zbc_test_count_conv_zones()
{
	nr_conv_zones=`zbc_zones | zbc_zone_filter_in_type "${ZT_CONV}" | wc -l`
}

function zbc_test_count_seq_zones()
{
	nr_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${ZT_SEQ}" | wc -l`
}

function zbc_test_count_seq_zones()
{
	nr_zones=`cat ${zone_info_file} | grep Sequential | wc -l`
}

function zbc_test_count_inactive_zones()
{
	nr_inactive_zones=`cat ${zone_info_file} | while IFS=, read a b c d; do echo $d; done | grep -c 0xc`
}

function zbc_test_open_nr_zones()
{
	local _zone_type="${ZT_SWR}"
	local _zone_cond="${ZC_CLOSED}|${ZC_EMPTY}"
	local -i count=0
	local -i open_num=${1}

	for _line in `zbc_zones | zbc_zone_filter_in_type "${_zone_type}" \
				| zbc_zone_filter_in_cond "${_zone_cond}"` ; do
		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		local zone_type=${3}
		local zone_cond=${4}
		local start_lba=${5}
		local zone_size=${6}
		local write_ptr=${7}

		IFS="$_IFS"

		if [ "${zone_cond}" == "0x1" -o "${zone_cond}" == "0x4" ]; then
		zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${start_lba}
		count=${count}+1
		if [ ${count} -eq $(( ${open_num} )) ]; then
			return 0
		fi
		fi

	done

	return 1
}

function zbc_test_close_nr_zones()
{
	local _zone_type="${ZT_SWR}"
	local _zone_cond="${ZC_EMPTY}"
	local -i count=0
	local -i close_num=${1}

	for _line in `zbc_zones | zbc_zone_filter_in_type "${_zone_type}" \
				| zbc_zone_filter_in_cond "${_zone_cond}"` ; do
		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		local zone_type=${3}
		local zone_cond=${4}
		local start_lba=${5}
		local zone_size=${6}
		local write_ptr=${7}

		IFS="$_IFS"

		zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${start_lba} ${lblk_per_pblk}
		zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${start_lba}

		count=${count}+1
		if [ ${count} -eq $(( ${close_num} )) ]; then
			return 0
		fi
	done

	return 1
}

function zbc_test_get_target_zone_from_type()
{

	local zone_type="${1}"

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}"`; do

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

function zbc_test_get_target_zone_from_slba()
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

function zbc_test_get_target_zone_from_type_and_cond()
{
	local zone_type="${1}"
	local zone_cond="${2}"

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}" \
				| zbc_zone_filter_in_cond "${zone_cond}"`; do

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

function zbc_test_get_target_zone_from_type_and_ignored_cond()
{

	local zone_type="${1}"
	local zone_cond_ignore="${2}"

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}" \
				| zbc_zone_filter_out_cond "${zone_cond_ignore}"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		target_type=${3}
		target_cond=${4}
		target_slba=${5}
		target_size=${6}
		target_ptr=${7}

		IFS="$_IFS"

		if [ "${zone_type}" = "${target_type}" ]; then
			if ! [[ "${target_cond}" =~ ^(${zone_cond})$ ]]; then
				return 0
			fi
		fi

	done

	return 1
}

function zbc_test_search_last_zone_vals_from_zone_type()
{
	local zone_type="${1}"

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}" | tail -n 1`; do

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

# Select a zone for testing and return info.
#
# If ${test_zone_type} is set, search for that; otherwise search for SWR|SWP.
# $1 is a regular expression denoting the desired zone condition.
# If $1 is omitted, a zone is matched if it is available (not OFFLINE, etc).
#
# Return info is the same as zbc_test_search_vals_*
# If no matching zone is found, exit with "N/A" message.
function zbc_test_get_zone_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_get_zone_info
	zbc_test_get_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		zbc_test_run ${bin_path}/zbc_test_reset_zone ${device} -1
		zbc_test_print_not_applicable \
		    "No zone is of type ${_zone_type} and condition ${_zone_cond}"
	fi
}

# Select a Write-Pointer zone for testing and return info.
# Argument and return information are the same as zbc_test_get_zone_or_NA.
function zbc_test_get_wp_zone_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"

	if [ "${_zone_type}" = "${ZT_CONV}" ]; then
		zbc_test_print_not_applicable \
			"Zone type ${_zone_type} is not a write-pointer zone type"
	fi

	zbc_test_get_zone_or_NA "$@"
}

# zbc_test_zone_tuple zone_type num_zones
# Returns the first zone of a contiguous sequence of length nz with the specified type.
# Returns non-zero if the request could not be met.
function zbc_test_zone_tuple()
{
	local zone_type="${1}"
	local -i nz=${2}

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		target_type=${3}
		target_cond=${4}
		target_slba=${5}
		target_size=${6}
		target_ptr=${7}

		IFS="$_IFS"

		local type=${target_type}
		local slba=${target_slba}
		local -i i
		for (( i=0 ; i<${nz} ; i++ )) ; do
			if [[ ${target_type} != ${type} ]]; then
				continue 2	# continue second loop out
			fi
			# Reject zones that are OFFLINE, etc
			if [[ ${target_cond} != @(${ZC_AVAIL}) ]]; then
				continue 2	# continue second loop out
			fi
			zbc_test_get_target_zone_from_slba $(( ${target_slba} + ${target_size} ))
		done

		# Return the info for the first zone of the tuple
		zbc_test_get_target_zone_from_slba ${slba}
		return 0
	done

	return 1
}

# zbc_test_zone_tuple_cond type cond1 [cond2...]
# Sets zbc_test_search_vals from the first zone of a
#	contiguous sequence with the specified type and conditions
# Return value is non-zero if the request cannot be met.
function zbc_test_zone_tuple_cond()
{
	local zone_type="${1}"
	shift
	local -i nzone=$#

	zbc_test_get_zone_info

	# Get ${nzone} zones in a row, all of the same ${target_type} matching ${zone_type}
	zbc_test_zone_tuple ${zone_type} ${nzone}
	if [ $? -ne 0 ]; then
		return 1
	fi
	local start_lba=${target_slba}

	# Set the zones to the requested conditions
	local -i zn
	for (( zn=0 ; zn<${nzone} ; zn++ )) ; do
		local cond="$1"
		case "${cond}" in
		"EMPTY")
			# RESET to EMPTY
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			;;
		"IOPENL")
			# IMPLICIT OPEN by writing the first ${lblk_per_pblk} LBA of the zone
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}
			;;
		"IOPENH")
			# IMPLICIT OPEN by writing all but the last ${lblk_per_pblk} LBA of the zone
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} $(( ${target_size} - ${lblk_per_pblk} ))
			;;
		"EOPEN")
			# EXPLICIT OPEN of an empty zone
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${target_slba}
			;;
		"FULL")
			# FULL by writing the entire zone
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${target_size}
			;;
		* )
			echo "Caller requested unsupported condition ${cond}"
			exit 1
			;;
		esac

		shift
		zbc_test_get_target_zone_from_slba $(( ${target_slba} + ${target_size} ))
	done

	# Return the info for the first zone of the tuple
	zbc_test_get_zone_info
	zbc_test_get_target_zone_from_slba ${start_lba}
	return 0
}

# Conversion domain manipulation functions

function zbc_test_get_cvt_domain_info()
{
	local _cmd="${bin_path}/zbc_test_domain_report ${device}"
	echo "" >> ${log_file} 2>&1
	echo "## Executing: ${_cmd} > ${cvt_domain_info_file} 2>&1" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1

	${_cmd} > ${cvt_domain_info_file} 2>&1

	return 0
}

function zbc_test_count_cvt_domains()
{
	nr_domains=`cat ${cvt_domain_info_file} | grep "\[CVT_DOMAIN_INFO\]" | wc -l`
}

function zbc_test_count_conv_domains()
{
	nr_conv_domains=`cat ${cvt_domain_info_file} | while IFS=, read a b c; do echo $c; done | grep -c 0x1`
}

function zbc_test_count_seq_domains()
{
	nr_seq_domains=`cat ${cvt_domain_info_file} | while IFS=, read a b c; do echo $c; done | grep -c 0x2`
}

function zbc_test_count_cvt_to_conv_domains()
{
	nr_cvt_to_conv_domains=`cat ${cvt_domain_info_file} | while IFS=, read a b c d e f g h i j; do echo $i; done | grep -c Y`
}

function zbc_test_count_cvt_to_seq_domains()
{
	nr_cvt_to_seq_domains=`cat ${cvt_domain_info_file} | while IFS=, read a b c d e f g h i j; do echo $j; done | grep -c Y`
}

function zbc_test_search_cvt_domain_by_number()
{
	domain_number=`printf "%03u" "${1}"`

	# [CVT_DOMAIN_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${cvt_domain_info_file} | grep "\[CVT_DOMAIN_INFO\],${domain_number},.*,.*,.*,.*,.*,.*,.*,.*"`; do

		_IFS="${IFS}"
		IFS=','
		set -- ${_line}

		domain_type=${3}
		domain_conv_start=${4}
		domain_conv_len=${5}
		domain_seq_start=${6}
		domain_seq_len=${7}
		domain_cvt_to_conv=${9}
		domain_cvt_to_seq=${10}

		IFS="$_IFS"

		return 0

	done

	return 1
}

function zbc_test_search_cvt_domain_by_type()
{
	domain_type=${1}
	_skip=$(expr ${2:-0})

	# [CVT_DOMAIN_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${cvt_domain_info_file} | grep "\[CVT_DOMAIN_INFO\],.*,0x${domain_type},.*,.*,.*,.*,.*,.*,.*"`; do

		if [ "${_skip}" -eq "0" ]; then

			_IFS="${IFS}"
			IFS=','
			set -- ${_line}

			domain_num=$(expr ${2} + 0)
			domain_conv_start=${4}
			domain_conv_len=${5}
			domain_seq_start=${6}
			domain_seq_len=${7}
			domain_cvt_to_conv=${9}
			domain_cvt_to_seq=${10}

			IFS="$_IFS"

			return 0

		else
			_skip=$(expr ${_skip} - 1)
		fi

	done

	return 1
}

function zbc_test_search_domain_by_type_and_cvt()
{
	domain_type=${1}
	_skip=$(expr ${3:-0})

	case "${2}" in
	"conv")
		cvt="Y,.*"
		;;
	"noconv")
		cvt="N,.*"
		;;
	"seq")
		cvt=".*,Y"
		;;
	"noseq")
		cvt=".*,N"
		;;
	"both")
		cvt="Y,Y"
		;;
	"none")
		cvt="N,N"
		;;
	* )
		exit 1
		;;
	esac

	# [CVT_DOMAIN_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${cvt_domain_info_file} | grep "\[CVT_DOMAIN_INFO\],.*,0x${domain_type},.*,.*,.*,.*,.*,${cvt}"`; do

		if [ "${_skip}" -eq "0" ]; then

			_IFS="${IFS}"
			IFS=','
			set -- ${_line}

			domain_num=$(expr ${2} + 0)
			domain_conv_start=${4}
			domain_conv_len=${5}
			domain_seq_start=${6}
			domain_seq_len=${7}
			domain_cvt_to_conv=${9}
			domain_cvt_to_seq=${10}

			IFS="$_IFS"

			return 0

		else
			_skip=$(expr ${_skip} - 1)
		fi

	done

	return 1
}

function zbc_test_calc_nr_domain_zones()
{
	domain_num=${1}
	_nr_domains=${2}
	nr_conv_zones=0
	nr_seq_zones=0

	for _line in `cat ${cvt_domain_info_file} | grep "\[CVT_DOMAIN_INFO\]*"`; do

		_IFS="${IFS}"
		IFS=','
		set -- ${_line}

		if [ "$(expr ${2} + 0)" -ge "${domain_num}" ]; then

			nr_conv_zones=$(expr ${nr_conv_zones} + ${5})
			nr_seq_zones=$(expr ${nr_seq_zones} + ${7})
			_nr_domains=$(expr ${_nr_domains} - 1)

		fi

		IFS="$_IFS"

		if [ "${_nr_domains}" -eq 0 ]; then
			return 0
		fi
	done

	return 1
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
	zbc_test_print_res "" " N/A  $1"
	exit
}

function zbc_test_print_failed()
{
	zbc_test_print_res "${red}" "Failed"
}

function zbc_test_print_failed_sk()
{
	zbc_test_print_res "${red}" "Failed"

	echo "=> Expected ${expected_sk} / ${expected_asc}, Got ${sk} / ${asc}" >> ${log_file} 2>&1
	echo "            => Expected ${expected_sk} / ${expected_asc}"
	echo "               Got ${sk} / ${asc}"

	if [ -n "$1" ]; then
		echo "           FAIL INFO: $*" | tee -a ${log_file}
	fi
}

function zbc_test_check_sk_ascq()
{
	if [ "${sk}" = "${expected_sk}" -a "${asc}" = "${expected_asc}" ]; then
		zbc_test_print_passed
	else
		zbc_test_print_failed_sk "$*"
	fi
}

function zbc_test_check_no_sk_ascq()
{
	local expected_sk=""
	local expected_asc=""
	if [ -z "${sk}" -a -z "${asc}" ]; then
		zbc_test_print_passed
	else
		zbc_test_print_failed_sk "$*"
	fi
}

function zbc_test_fail_if_sk_ascq()
{
	local expected_sk=""
	local expected_asc=""
	if [ -n "${sk}" -o -n "${asc}" ]; then
		zbc_test_print_failed_sk "$*"
	fi
}

function zbc_test_print_failed_zc()
{
	zbc_test_print_res "${red}" "Failed"

	echo "=> Expected zone condition ${expected_cond}, Got ${target_cond}" >> ${log_file} 2>&1
	echo "            => Expected zone condition ${expected_cond}"
	echo "               Got ${target_cond}"

	if [ -n "$1" ]; then
		echo "           FAIL INFO: $*" | tee -a ${logfile}
	fi
}

function zbc_test_check_zone_cond()
{
	local expected_sk=""
	local expected_asc=""

	# Check sk_ascq first
	if [ -n "${sk}" -o -n "${asc}" ]; then
		zbc_test_print_failed_sk "$*"
        elif [ "${target_cond}" != "${expected_cond}" ]; then
		zbc_test_print_failed_zc "$*"
        else
                zbc_test_print_passed
        fi
}

function zbc_test_dump_zone_info()
{
	zbc_report_zones ${device} > ${dump_zone_info_file}
}

function zbc_test_dump_cvt_domain_info()
{
	zbc_domain_report ${device} > ${dump_cvt_domain_info_file}
}

function zbc_test_check_failed()
{

	failed=`cat ${log_file} | grep -m 1 "^Failed"`

	if [ "Failed" = "${failed}" ]; then
		zbc_test_dump_zone_info
		if [ "${zone_activation_device}" != "0" ]; then
			zbc_test_dump_cvt_domain_info
		fi
		return 1
	fi

	return 0
}

