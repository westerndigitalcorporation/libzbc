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

# Zone Type                   is a ...  Write Pointer zone   Sequential zone
# ---------                             ------------------   ---------------
# 0x1 Conventional                              NO                 NO
# 0x4 Sequential or Before Req'd (SBR)          YES                NO
# 0x3 Sequential Write Preferred (SWP)          YES                YES
# 0x2 Sequential Write Required  (SWR)          YES                YES

# For pretty printing...
red="\e[1;31m"
green="\e[1;32m"
end="\e[m"

function zbc_test_lib_init()
{
	# Expected Sense strings for ACTIVATE/QUERY status returns
	declare -rg ERR_ZA_SK="Unknown-sense-key 0x00"
	declare -rg ERR_ZA_ASC="Unknown-additional-sense-code-qualifier 0x00"

	# Zone types with various attributes
	declare -rg ZT_CONV="0x1"			# Conventional zone
	declare -rg ZT_SWR="0x2"			# Sequential Write Required zone
	declare -rg ZT_SWP="0x3"			# Sequential Write Preferred zone
	declare -rg ZT_WPC="0x4"			# Write-Pointer Conventional zone XXX
	declare -rg ZT_SBR="0x4"			# Sequential or Before Required zone

	# Example Usage:  if [[ ${target_type} == @(${ZT_NON_SEQ}) ]]; then...
	#                 if [[ ${target_type} != @(${ZT_WP}) ]]; then...

	declare -rg ZT_NON_SEQ="${ZT_CONV}|${ZT_WPC}"	# CMR
	declare -rg ZT_SEQ="${ZT_SWR}|${ZT_SWP}"	# SMR
	declare -rg ZT_WP="${ZT_SEQ}|${ZT_WPC}"		# Write Pointer zone

	declare -rg ZT_DISALLOW_WRITE_XTYPE="0x1|0x2|0x3|0x4"	# Write across zone types disallowed XXX
	declare -rg ZT_DISALLOW_WRITE_GT_WP="0x2|0x4"	# Write starting above WP disallowed
	declare -rg ZT_REQUIRE_WRITE_PHYSALIGN="0x2|0x4" # Write ending >= WP must be physical-block-aligned
	declare -rg ZT_DISALLOW_READ_XTYPE="0x1|0x2|0x3|0x4"	# Read across zone types disallowed XXX
	declare -rg ZT_RESTRICT_READ_GE_WP="0x2|0x4"	# Read ending above WP disallowed when !URSWRZ

	declare -rg ZT_DISALLOW_WRITE_XZONE="0x2"	# Write across zone boundary disallowed
	declare -rg ZT_DISALLOW_WRITE_LT_WP="0x2"	# Write starting below WP disallowed
	declare -rg ZT_DISALLOW_WRITE_FULL="0x2"	# Write FULL zone disallowed
	declare -rg ZT_W_OZR="0x2"			# Participates in Open Zone Resources protocol
	declare -rg ZT_RESTRICT_READ_XZONE="0x2"	# Read across zone boundary disallowed when !URSWRZ

	# Zone conditions
	declare -rg ZC_NOT_WP="0x0"			# NOT_WRITE_POINTER zone condition
	declare -rg ZC_EMPTY="0x1"			# EMPTY zone condition
	declare -rg ZC_IOPEN="0x2"			# IMPLICITLY OPEN zone condition
	declare -rg ZC_EOPEN="0x3"			# EXPLICITLY OPEN zone condition
	declare -rg ZC_OPEN="${ZC_IOPEN}|${ZC_EOPEN}"	# Either OPEN zone condition
	declare -rg ZC_CLOSED="0x4"			# CLOSED zone condition
	declare -rg ZC_FULL="0xe"			# FULL zone condition
	declare -rg ZC_RDONLY="0xd"			# READ ONLY zone condition
	declare -rg ZC_INACTIVE="0xc"			# INACTIVE zone condition
	declare -rg ZC_NON_FULL="0x0|0x1|0x2|0x3|0x4"	# Non-FULL available zone conditions
	declare -rg ZC_AVAIL="${ZC_NON_FULL}|${ZC_FULL}" # available zone conditions
}

if [ -z "${ZBC_TEST_LIB_INIT}" ]; then
	zbc_test_lib_init
	ZBC_TEST_LIB_INIT=1
fi

function _stacktrace()
{
	local RET=
	local -i FRAME=2
	local STR=${FUNCNAME[${FRAME}]}
	while [ ! -z ${STR} ] ; do
		RET+=" ${STR}:${BASH_LINENO[$((${FRAME}-1))]}"
		FRAME=$(( ${FRAME} + 1 ))
		STR=${FUNCNAME[${FRAME}]}
		if [ -z ${FUNCNAME[$((${FRAME}+1))]} ] ; then break; fi
	done
	echo ${RET}
}

function _stacktrace_exit()
{
	echo "$* (FAIL from `_stacktrace`)"
	exit 1
}

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

	# Zone realm info file
	zone_realm_info_file="/tmp/${case_num}_zone_realm_info.`basename ${device}`.log"
	rm -f ${zone_realm_info_file}

	# Dump zone info file
	dump_zone_info_file="${log_path}/${case_num}_zone_info.log"

	# Dump zone realm info file
	dump_zone_realm_info_file="${log_path}/${case_num}_zone_realm_info.log"
}

# Reset the DUT to factory conditions
function zbc_reset_test_device()
{
	local _IFS="${IFS}"
	local vendor=`zbc_info ${device} | grep "Vendor ID: .*" | while IFS=: read a b; do echo $b; done`
	IFS="$_IFS"

	case "${vendor}" in
	"LIO-ORG TCMU DH-SMR" )
		echo "Resetting the device..."
		sg_sanitize -CQw ${device}
		;;
	* )
		if [ ${format_dut} -eq 1 ]; then
			echo "Formatting the device..."
			sg_format ${device}
		fi
		;;
	esac

	return $?
}

function zbc_test_reset_device()
{
	zbc_reset_test_device
	if [ $? -ne 0 ]; then
		_stacktrace_exit "Can't reset test device"
	fi

	zbc_test_get_device_info

	local reason=""

	if [ ${maxact_control} -ne 0 ]; then
		# Allow the main ACTIVATE tests to run unhindered
		zbc_test_run ${bin_path}/zbc_test_dev_control -maxd unlimited ${device}
		if [ $? -ne 0 ]; then
			reason="because \'zbc_test_dev_control -maxd unlimited\' failed"
		else
			reason="even though \'zbc_test_dev_control -maxd unlimited\' returned success"
		fi
	else
		reason="because maxact_control=${maxact_control}"
	fi

	local max_act=`zbc_info ${device} | grep "Maximum number of zones to activate" | sed -e "s/.* //"`  #XXX
	if [ -n "${max_act}" -a "${max_act}" != "unlimited" ]; then
		echo "WARNING: zbc_test_reset_device did not set (max_conversion=${max_act}) to unlimited ${reason}" `_stacktrace`
	fi

	zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} -1
	if [ $? -ne 0 ]; then
		echo "WARNING: zbc_test_reset_device failed to zone_reset ALL" `_stacktrace`
	fi
}

function zbc_test_run()
{
	local _cmd="$*"

	echo "" >> ${log_file} 2>&1
	echo "## Executing: ${_cmd}" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1
	${VALGRIND} ${_cmd} >> ${log_file} 2>&1

	return $?
}

function zbc_test_meta_run()
{
	local _cmd="$*"

	export ZBC_TEST_LOG_PATH_BASE
	export ZBC_TEST_SECTION_LIST
	export ZBC_TEST_FORCE_ATA
	export CHECK_ZC_BEFORE_ZT
	export VALGRIND
	export ATA_SCSI_NONSENSE

	echo -e "\n### Executing: ${_cmd}\n" 2>&1 | tee -a ${log_file} 2>&1

	${_cmd} 2>&1 | tee -a ${log_file} 2>&1
	local ret=${PIPESTATUS[0]}

	return 0
#	return ${ret}	# seems more annoying than helpful
}

# Get information functions

function zbc_check_string()
{
	if [ -z "$2" ]; then
		echo "$1"
		_stacktrace_exit
	fi
}

function zbc_test_get_device_info()
{
	if [ -z "${device}" ]; then
		_stacktrace_exit "device is not set -- call zbc_test_init before zbc_test_get_device_info"
	fi

	zbc_test_run ${bin_path}/zbc_test_print_devinfo ${device}
	if [ $? -ne 0 ]; then
		_stacktrace_exit "Failed to get device info for ${device}"
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

	max_rw_sectors_line=`cat ${log_file} | grep -F "[MAX_RW_SECTORS]"`
	set -- ${max_rw_sectors_line}
	max_rw_sectors=${2}
	zbc_check_string "Failed to get maximum Read/Write size" ${max_rw_sectors}

	max_rw_lba=$(( ${max_rw_sectors} * 512 / ${logical_block_size} ))

	local unrestricted_read_line=`cat ${log_file} | grep -F "[URSWRZ]"`
	set -- ${unrestricted_read_line}
	unrestricted_read=${2}
	zbc_check_string "Failed to get unrestricted read" ${unrestricted_read}

	local zone_domains_device_line=`cat ${log_file} | grep -F "[ZONE_DOMAINS_DEVICE]"`
	set -- ${zone_domains_device_line}
	zone_domains_device=${2}
	zbc_check_string "Failed to get Zone Domains device support" ${zone_domains_device}

	if [ ${zone_domains_device} -ne 0 ]; then

		local ur_control_line=`cat ${log_file} | grep -F "[UR_CONTROL]"`
		set -- ${ur_control_line}
		ur_control=${2}
		zbc_check_string "Failed to get unrestricted read control" ${ur_control}

		local report_realms_line=`cat ${log_file} | grep -F "[REPORT_REALMS]"`
		set -- ${report_realms_line}
		report_realms=${2}
		zbc_check_string "Failed to get REPORT REALMS support" ${report_realms}

		local zone_query_line=`cat ${log_file} | grep -F "[ZONE_QUERY]"`
		set -- ${zone_query_line}
		zone_query=${2}
		zbc_check_string "Failed to get ZONE QUERY support" ${zone_query}

		local za_control_line=`cat ${log_file} | grep -F "[ZA_CONTROL]"`
		set -- ${za_control_line}
		za_control=${2}
		zbc_check_string "Failed to get zone activation control" ${za_control}

		local maxact_control_line=`cat ${log_file} | grep -F "[MAXACT_CONTROL]"`
		set -- ${maxact_control_line}
		maxact_control=${2}
		zbc_check_string "Failed to get maximum activation realms control" ${maxact_control}

		local conv_zone_line=`cat ${log_file} | grep -F "[CONV_ZONE]"`
		set -- ${conv_zone_line}
		conv_zone=${2}
		zbc_check_string "Failed to get Conventional zone support" ${conv_zone}

		local seq_req_zone_line=`cat ${log_file} | grep -F "[SEQ_REQ_ZONE]"`
		set -- ${seq_req_zone_line}
		seq_req_zone=${2}
		zbc_check_string "Failed to get Sequential Write Required zone support" ${seq_req_zone}

		local seq_pref_zone_line=`cat ${log_file} | grep -F "[SEQ_PREF_ZONE]"`
		set -- ${seq_pref_zone_line}
		seq_pref_zone=${2}
		zbc_check_string "Failed to get Sequential Write Preferred zone support" ${seq_pref_zone}

		local wpc_zone_line=`cat ${log_file} | grep -F "[WPC_ZONE]"`
		set -- ${wpc_zone_line}
		wpc_zone=${2}
		sbr_zone=${2}
		zbc_check_string "Failed to get Sequential or Before Required zone support" ${wpc_zone}
	else
		ur_control=0
		report_realms=0
		zone_query=0
		za_control=0
		maxact_control=0
		wpc_zone=0
		conv_zone=1
		if [ "${device_model}" = "Host-aware" ]; then
			seq_pref_zone=1
			seq_req_zone=0
		else
			seq_pref_zone=0
			seq_req_zone=1
		fi
	fi

	local last_zone_lba_line=`cat ${log_file} | grep -F "[LAST_ZONE_LBA]"`
	set -- ${last_zone_lba_line}
	last_zone_lba=${2}
	zbc_check_string "Failed to get last zone start LBA" ${last_zone_lba}

	local last_zone_size_line=`cat ${log_file} | grep -F "[LAST_ZONE_SIZE]"`
	set -- ${last_zone_size_line}
	last_zone_size=${2}
	zbc_check_string "Failed to get last zone size" ${last_zone_size}

	local mutate_line=`cat ${log_file} | grep -F "[MUTATE]"`
	set -- ${mutate_line}
	mutations=${2}
	zbc_check_string "Failed to get mutation support" ${mutations}

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

	${VALGRIND} ${_cmd} > ${zone_info_file} 2>> ${log_file}

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
	nr_zones=`zbc_zones | wc -l`
}

function zbc_test_count_conv_zones()
{
	nr_conv_zones=`zbc_zones | zbc_zone_filter_in_type "${ZT_CONV}" | wc -l`
}

function zbc_test_count_seq_zones()
{
	nr_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${ZT_SEQ}" | wc -l`
}

function zbc_test_count_inactive_zones()
{
	nr_inactive_zones=`zbc_zones | zbc_zone_filter_in_cond "${ZC_INACTIVE}" | wc -l`
}

# Set expected errors if zone is not available for write
function write_check_available()
{
    local target_cond="$1"
    #XXX Emulator may check these zone conditions before boundary checks
    if [ -n "${CHECK_ZC_BEFORE_ZT}" -a ${CHECK_ZC_BEFORE_ZT} -ne 0 ]; then
        if [ "${target_cond}" = "${ZC_INACTIVE}" ]; then
           expected_sk="Aborted-command"
           expected_asc="Zone-is-inactive"
        elif [ "${target_cond}" = "0xf" ]; then
           expected_sk="Data-protect"
           expected_asc="Zone-is-offline"
        elif [ "${target_cond}" = "0xd" ]; then
           expected_sk="Data-protect"
           expected_asc="Zone-is-read-only"
        fi
    fi
}

# Set expected errors if zone is not available for read
function read_check_available()
{
    local target_cond="$1"
    #XXX Emulator may check these zone conditions before boundary checks
    if [ -n "${CHECK_ZC_BEFORE_ZT}" -a ${CHECK_ZC_BEFORE_ZT} -ne 0 ]; then
        if [ "${target_cond}" = "${ZC_INACTIVE}" ]; then
           expected_sk="Aborted-command"
           expected_asc="Zone-is-inactive"
        elif [ "${target_cond}" = "0xf" ]; then
           expected_sk="Data-protect"		# ZBC 5.3(b)(B)
           expected_asc="Zone-is-offline"
        fi
    fi
}

# $1 is type of zones to open; $2 is number of zones to open
# It is expected that the requested number can be opened
function zbc_test_open_nr_zones()
{
	local _zone_cond="${ZC_EMPTY}"
	local _zone_type="${1}"
	local -i _open_num=${2}
	local -i _count=0

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

		zbc_test_run ${bin_path}/zbc_test_open_zone -v ${device} ${start_lba}
		if [ $? -ne 0 ]; then
			echo "WARNING: Unexpected failure to open zone ${start_lba} after ${_count}/${_open_num} opens"
			zbc_test_dump_zone_info
			return 1
		fi

		_count=${_count}+1

		if [ ${_count} -ge ${_open_num} ]; then
			return 0
		fi
	done

	zbc_test_dump_zone_info
	return 1
}

function zbc_test_close_nr_zones()
{
	local _zone_cond="${ZC_EMPTY}"
	local _zone_type="${1}"
	local -i _close_num=${2}
	local -i _count=0

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
		if [ $? -ne 0 ]; then
			echo "WARNING: Unexpected failure to write zone ${start_lba} after writing ${_count}/${_close_num} zones"
			zbc_test_dump_zone_info
			return 1
		fi

		zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${start_lba}
		if [ $? -ne 0 ]; then
			echo "WARNING: Unexpected failure to close zone ${start_lba}"
			zbc_test_dump_zone_info
			return 1
		fi

		_count=${_count}+1

		if [ ${_count} -ge ${_close_num} ]; then
			return 0
		fi
	done

	zbc_test_dump_zone_info
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

		# Warning: ${2} is *not* the zone number, merely the index in the current report
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

# These functions look for a zone aleady in the condition

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

function UNUSED__zbc_test_get_target_zone_from_type_and_ignored_cond()
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

		return 0

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
function zbc_test_search_zone_cond()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_get_zone_info

	zbc_test_get_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		return 1
	fi

	return 0
}
function zbc_test_search_zone_cond_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_get_zone_info
	zbc_test_get_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		zbc_test_print_not_applicable \
		    "No zone is of type ${_zone_type} and condition ${_zone_cond}"
	fi
}

# Select a Write-Pointer zone for testing and return info.
# Argument and return information are the same as zbc_test_search_zone_cond_or_NA.
function zbc_test_search_wp_zone_cond_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"

	if [ "${_zone_type}" = "${ZT_CONV}" ]; then
		zbc_test_print_not_applicable \
		    "Zone type ${_zone_type} is not a write-pointer zone type"
	fi

	zbc_test_search_zone_cond_or_NA "$@"
}

# Select a non-Sequential zone for testing and return info.
# Argument and return information are the same as zbc_test_search_zone_cond_or_NA.
function zbc_test_search_non_seq_zone_cond_or_NA()
{
	local _zone_type="${ZT_NON_SEQ}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_get_zone_info
	zbc_test_get_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		zbc_test_print_not_applicable \
		    "No zone is of type ${_zone_type} and condition ${_zone_cond}"
	fi
}

# Select a Sequential zone for testing and return info.
# Argument and return information are the same as zbc_test_search_zone_cond_or_NA.
function zbc_test_search_seq_zone_cond_or_NA()
{
	local _zone_type="${ZT_SEQ}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_get_zone_info
	zbc_test_get_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		zbc_test_print_not_applicable \
		    "No zone is of type ${_zone_type} and condition ${_zone_cond}"
	fi
}

# zbc_test_get_zones zone_type num_zones
# Returns the first zone of a contiguous sequence of length nz with the specified type.
# Returns non-zero if the request could not be met.
function zbc_test_get_zones()
{
	local zone_type="${1}"
	local -i nz=${2}


	zbc_test_get_zone_info

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

# These functions set the zone(s) to the specified condition(s)

# zbc_test_get_zones_cond type cond1 [cond2...]
# Sets zbc_test_search_vals from the first zone of a
#	contiguous sequence with the specified type and conditions
# Return value is non-zero if the request cannot be met.
function zbc_test_get_zones_cond()
{
	local zone_type="${1}"
	shift
	local -i nzone=$#

	# Get ${nzone} zones in a row, all of the same ${target_type} matching ${zone_type}
	zbc_test_get_zones ${zone_type} ${nzone}
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
		"IOPENZ")
			# IMPLICIT OPEN by writing zero LBA to the zone
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} 0
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
			_stacktrace_exit "Caller requested unsupported condition ${cond}"
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

function zbc_test_get_seq_zones_cond_or_NA()
{
	zbc_test_get_zones_cond ${ZT_SEQ} "$@"
	if [ $? -ne 0 ]; then
	    if [ $# -gt 1 ]; then
	        zbc_test_print_not_applicable "No available sequential zone sequence of length $#"
	    else
	        zbc_test_print_not_applicable "No available sequential zone"
	    fi
	fi
}

# zbc_test_get_wp_zones_cond_or_NA cond1 [cond2...]
# Sets zbc_test_search_vals from the first zone of a
#	contiguous sequence with the specified type and conditions
# If ${test_zone_type} is set, search for that; otherwise search for SWR|SWP.
# If ${test_zone_type} is set, it should refer (only) to one or more WP zones.
# Exits with "N/A" message if the request cannot be met
function zbc_test_get_wp_zones_cond_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"

	if [ "${_zone_type}" = "${ZT_CONV}" ]; then
		zbc_test_print_not_applicable \
			"Zone type ${_zone_type} is not a write-pointer zone type"
	fi

	zbc_test_get_zones_cond "${_zone_type}" "$@"
	if [ $? -ne 0 ]; then
	    if [ $# -gt 1 ]; then
		zbc_test_print_not_applicable \
		    "No available write-pointer zone sequence of type ${_zone_type} and length $#"
	    else
	        zbc_test_print_not_applicable \
		    "No available write-pointer zone of type ${_zone_type}"
	    fi
	fi
}

# Zone realm manipulation functions

function zbc_test_get_zone_realm_info()
{
	local _cmd="${bin_path}/zbc_test_report_realms ${device}"
	echo "" >> ${log_file} 2>&1
	echo "## Executing: ${_cmd} > ${zone_realm_info_file} 2>&1" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1

	${VALGRIND} ${_cmd} > ${zone_realm_info_file} 2>> ${log_file}

	return 0
}

function zbc_test_count_zone_realms()
{
	nr_realms=`cat ${zone_realm_info_file} | grep "\[ZONE_REALM_INFO\]" | wc -l`
}

function UNUSED_zbc_test_count_zone_realms()
{
	local _IFS="${IFS}"
	nr_zone_realms=`cat ${zone_realm_info_file} | while IFS=, read a b c; do echo $c; done | grep -c -E "(${ZT_NON_SEQ})"`
	IFS="$_IFS"
}

function UNUSED_zbc_test_count_seq_realms()
{
	local _IFS="${IFS}"
	nr_seq_realms=`cat ${zone_realm_info_file} | while IFS=, read a b c; do echo $c; done | grep -c -E "(${ZT_SEQ})"`
	IFS="$_IFS"
}

function zbc_test_count_cvt_to_conv_realms()
{
	local _IFS="${IFS}"
	nr_cvt_to_conv_realms=`cat ${zone_realm_info_file} | while IFS=, read a b c d e f g h i j; do echo $i; done | grep -c Y`
	IFS="$_IFS"
}

function zbc_test_count_cvt_to_seq_realms()
{
	local _IFS="${IFS}"
	nr_cvt_to_seq_realms=`cat ${zone_realm_info_file} | while IFS=, read a b c d e f g h i j; do echo $j; done | grep -c Y`
	IFS="$_IFS"
}

# Return non-zero if a realm found by zbc_test_search_* has zones R/O or offline
function zbc_test_is_found_realm_faulty()
{
	local _target_slba
	zbc_test_get_zone_info

	zbc_test_get_target_zone_from_slba ${realm_seq_start}
	for (( i=0 ; i<${realm_seq_len} ; i++ )) ; do
		if [ $? -ne 0 ]; then
			break
		fi
		if [ ${target_size} -eq 0 ]; then
			break
		fi
		if [[ ${target_cond} == @(0xd|0xf) ]]; then
			return 1
		fi
		_target_slba=$(( ${target_slba} + ${target_size} ))
		zbc_test_get_target_zone_from_slba ${_target_slba}
	done

	zbc_test_get_target_zone_from_slba ${realm_conv_start}
	for (( i=0 ; i<${realm_conv_len} ; i++ )) ; do
		if [ $? -ne 0 ]; then
			break
		fi
		if [ ${target_size} -eq 0 ]; then
			break
		fi
		if [[ ${target_cond} == @(0xd|0xf) ]]; then
			return 1
		fi
		_target_slba=$(( ${target_slba} + ${target_size} ))
		zbc_test_get_target_zone_from_slba ${_target_slba}
	done

	return 0
}

function zbc_test_search_zone_realm_by_number()
{
	local realm_number=`printf "%03u" "${1}"`

	# [ZONE_REALM_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${zone_realm_info_file} | grep -E "\[ZONE_REALM_INFO\],(${realm_number}),.*,.*,.*,.*,.*,.*,.*,.*"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		realm_type=${3}
		realm_conv_start=`_trim ${4}`
		realm_conv_len=${5}
		realm_seq_start=`_trim ${6}`
		realm_seq_len=${7}
		realm_cvt_to_conv=${9}
		realm_cvt_to_seq=${10}

		IFS="$_IFS"

		return 0

	done

	return 1
}

function UNUSED_zbc_test_search_zone_realm_by_type()
{
	local realm_search_type=${1}
	local -i _skip=$(expr ${2:-0})

	# [ZONE_REALM_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${zone_realm_info_file} | grep -E "\[ZONE_REALM_INFO\],.*,(${realm_search_type}),.*,.*,.*,.*,.*,.*,.*"`; do

		if [ ${_skip} -eq 0 ]; then

			local _IFS="${IFS}"
			IFS=$',\n'
			set -- ${_line}

			realm_num=$(expr ${2} + 0)
			realm_type=${3}
			realm_conv_start=`_trim ${4}`
			realm_conv_len=${5}
			realm_seq_start=`_trim ${6}`
			realm_seq_len=${7}
			realm_cvt_to_conv=${9}
			realm_cvt_to_seq=${10}

			IFS="$_IFS"

			zbc_test_is_found_realm_faulty
			if [ $? -eq 0 ]; then
				return 0
			fi

		else
			_skip=$(( ${_skip} - 1 ))
		fi

	done

	return 1
}

# $1 is realm type, $2 is convertible_to
# Optional $3 = "NOFAULTY" specifies to skip faulty realms
function zbc_test_search_realm_by_type_and_cvt()
{
	local realm_search_type=${1}
	local -i _skip=0	# _skip=$(expr ${3:-0})
	local _NOFAULTY="$3"
	local cvt

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
		_stacktrace_exit "zbc_test_search_realm_by_type_and_cvt bad cvt arg=\"$2\""
		;;
	esac

	# [ZONE_REALM_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${zone_realm_info_file} | grep -E "\[ZONE_REALM_INFO\],.*,(${realm_search_type}),.*,.*,.*,.*,.*,${cvt}"`; do

		if [ ${_skip} -eq 0 ]; then

			local _IFS="${IFS}"
			IFS=$',\n'
			set -- ${_line}

			realm_num=$(expr ${2} + 0)
			realm_type=${3}
			realm_conv_start=`_trim ${4}`
			realm_conv_len=${5}
			realm_seq_start=`_trim ${6}`
			realm_seq_len=${7}
			realm_cvt_to_conv=${9}
			realm_cvt_to_seq=${10}

			IFS="$_IFS"

			if [ "${_NOFAULTY}" != "NOFAULTY" ]; then
				return 0
			fi

			zbc_test_is_found_realm_faulty
			if [ $? -eq 0 ]; then
				return 0
			fi

		else
			_skip=$(( ${_skip} - 1 ))
		fi

	done

	return 1
}

function zbc_test_calc_nr_realm_zones()
{
	local realm_num=${1}
	local -i _nr_realms=${2}
	nr_conv_zones=0
	nr_seq_zones=0

	# [ZONE_REALM_INFO],<num>,<type>,<conv_start>,<conv_len>,<seq_start>,<seq_len>,<ko>,<to_conv>,<to_seq>
	for _line in `cat ${zone_realm_info_file} | grep "\[ZONE_REALM_INFO\]"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		if [ $(expr ${2} + 0) -ge ${realm_num} ]; then

			nr_conv_zones=$(expr ${nr_conv_zones} + ${5})
			nr_seq_zones=$(expr ${nr_seq_zones} + ${7})
			_nr_realms=$(( ${_nr_realms} - 1 ))

		fi

		IFS="$_IFS"

		if [ ${_nr_realms} -eq 0 ]; then
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
	err_za=""
	err_cbf=""

	local _IFS="${IFS}"
	IFS=$',\n'

	local sk_line=`cat ${log_file} | grep -m 1 -F "[SENSE_KEY]"`
	set -- ${sk_line}
	sk=${2}

	local asc_line=`cat ${log_file} | grep -m 1 -F "[ASC_ASCQ]"`
	set -- ${asc_line}
	asc=${2}

	local err_za_line=`cat ${log_file} | grep -m 1 -F "[ERR_ZA]"`
	set -- ${err_za_line}
	err_za=${2}

	local err_cbf_line=`cat ${log_file} | grep -m 1 -F "[ERR_CBF]"`
	set -- ${err_cbf_line}
	err_cbf=${2}

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
	zbc_test_print_res "" " N/A  $*"
	exit 0
}

function zbc_test_print_failed()
{
	zbc_test_print_res "${red}" "Failed  $*"
}

function zbc_test_print_failed_sk()
{
	zbc_test_print_res "${red}" "Failed - See ${log_file}"

	if [ -z "${err_za}" -a -z "${err_cbf}" -a  -z "${expected_err_za}" -a -z "${expected_err_cbf}" ] ; then
		echo "=> Expected ${expected_sk} / ${expected_asc}, Got ${sk} / ${asc}" >> ${log_file} 2>&1
		echo "FAIL        => Expected ${expected_sk} / ${expected_asc}"
		echo "FAIL                Got ${sk} / ${asc}"
	else
		echo "=> Expected ${expected_sk} / ${expected_asc} (ZA-status: ${expected_err_za} / ${expected_err_cbf})"  >> ${log_file} 2>&1
		echo "	      Got ${sk} / ${asc} (ZA-status: ${err_za} / ${err_cbf})" >> ${log_file} 2>&1
		echo "FAIL        => Expected ${expected_sk} / ${expected_asc} (ZA-status: ${expected_err_za} / ${expected_err_cbf})"
		echo "FAIL                Got ${sk} / ${asc} (ZA-status: ${err_za} / ${err_cbf})"
	fi

	if [ -n "$1" ]; then
		echo "           FAIL INFO: $*" | tee -a ${log_file}
	fi
}

ata_nonsense_sk="Aborted-command"
ata_nonsense_asc="Unknown-additional-sense-code-qualifier 0x00"

function zbc_test_print_passed_except_sense()
{
	zbc_test_print_res "${green}" "Passed (except sense data)"
}

function zbc_test_check_err()
{
	if [ -n "${ATA_SCSI_NONSENSE}" ]; then
		if [ "${sk}" = "${ata_nonsense_sk}" -a "${asc}" = "${ata_nonsense_asc}" ]; then
			zbc_test_print_passed_except_sense
			return
		fi
	fi

	if [ -n "${expected_err_za}" -a -z "${expected_err_cbf}" ] ; then
		# Our caller expects ERR_ZA, but specified no expected CBF -- assume zero
		local expected_err_cbf=0	# expect (CBF == 0)
	fi

	if [ "${sk}" = "${expected_sk}" -a "${asc}" = "${expected_asc}" \
			-a "${err_za}" = "${expected_err_za}" -a "${err_cbf}" = "${expected_err_cbf}" ]; then
		zbc_test_print_passed
	else
		zbc_test_print_failed_sk "$*"
	fi
}

function zbc_test_check_sk_ascq()
{
	if [ -n "${ATA_SCSI_NONSENSE}" ]; then
		if [ "${sk}" = "${ata_nonsense_sk}" -a "${asc}" = "${ata_nonsense_asc}" ]; then
			zbc_test_print_passed_except_sense
			return
		fi
	fi

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
		return 1
	fi
	return 0
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

# zbc_test_check_wp_eq expected_wp err_msg
function zbc_test_check_wp_eq()
{
	local -i expected_wp=$1
	shift
	if [ ${target_ptr} -ne ${expected_wp} ]; then
		zbc_test_print_failed "Zone WP=${target_ptr} does not equal expected WP=${expected_wp} -- $*"
		return 1
	fi
	return 0
}

# zbc_test_check_wp_inrange wp_min wp_max err_msg
function zbc_test_check_wp_inrange()
{
	local -i wp_min=$1
	shift
	local -i wp_max=$1
	shift
	if [ ${target_ptr} -lt ${wp_min} -o ${target_ptr} -gt ${wp_max} ]; then
		zbc_test_print_failed "Zone WP=${target_ptr} is not within expected range [${wp_min}, ${wp_max}] -- $*"
		return 1
	fi
	return 0
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
		# For zone conditions with valid WP, check within zone range
		if [[ "${expected_cond}" == @(${ZC_EMPTY}) ]]; then
			zbc_test_check_wp_eq ${target_slba}
			if [ $? -ne 0 ]; then return; fi
		elif [[ "${expected_cond}" == @(${ZC_CLOSED}) ]]; then
			zbc_test_check_wp_inrange \
				      $(( ${target_slba} + 1 )) \
				      $(( ${target_slba} + ${target_size} - 1 ))
			if [ $? -ne 0 ]; then return; fi
		elif [[ "${expected_cond}" == @(${ZC_OPEN}) ]]; then
			zbc_test_check_wp_inrange \
				      ${target_slba} \
				      $(( ${target_slba} + ${target_size} - 1 ))
			if [ $? -ne 0 ]; then return; fi
		fi
		zbc_test_print_passed
	fi
}

# zbc_test_check_zone_cond_wp expected_wp err_msg
function zbc_test_check_zone_cond_wp()
{
	local expected_sk=""
	local expected_asc=""
	local -i expected_wp=$1
	shift

	# Check sk_ascq first
	if [ -n "${sk}" -o -n "${asc}" ]; then
		zbc_test_print_failed_sk "$*"
	elif [ "${target_cond}" != "${expected_cond}" ]; then
		zbc_test_print_failed_zc "$*"
	elif [ ${target_ptr} -ne ${expected_wp} ]; then
		zbc_test_print_failed "Zone WP=${target_ptr} does not equal expected WP=${expected_wp} -- $*"
	else
                zbc_test_print_passed
        fi
}

function zbc_test_dump_zone_info()
{
	zbc_report_zones ${device} > ${dump_zone_info_file}
}

function zbc_test_dump_zone_realm_info()
{
	zbc_report_realms ${device} > ${dump_zone_realm_info_file}
}

# Dump info files after a failed test -- returns 1 if test failed
function zbc_test_check_failed()
{
	failed=`cat ${log_file} | grep -m 1 "TESTRESULT==Failed"`
	if [[ ! -z "${failed}" ]]; then
		zbc_test_dump_zone_info
		if [ ${zone_domains_device} -ne 0 ]; then
			zbc_test_dump_zone_realm_info
		fi
		return 1
	fi

	return 0
}
