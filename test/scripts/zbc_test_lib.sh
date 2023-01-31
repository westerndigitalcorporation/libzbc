# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

# Zone Type                   is a ...  Write Pointer zone   Sequential zone
# ---------                             ------------------   ---------------
# 0x1 Conventional                              NO                 NO
# 0x4 Sequential Or Before Required (SOBR)      YES                NO
# 0x3 Sequential Write Preferred (SWP)          YES                YES
# 0x2 Sequential Write Required  (SWR)          YES                YES

# SCRIPT_DEBUG=1

if [[ -z "${SCRIPT_DEBUG}" ]]; then
    SCRIPT_DEBUG=0
fi

# For pretty printing...
red="\e[1;31m"
green="\e[1;32m"
end="\e[m"

function _zbc_test_lib_init()
{
	# Zone types with various attributes
	declare -rg ZT_CONV="0x1"			# Conventional zone
	declare -rg ZT_SWR="0x2"			# Sequential Write Required zone
	declare -rg ZT_SWP="0x3"			# Sequential Write Preferred zone
	declare -rg ZT_SOBR="0x4"			# Sequential Or Before Required zone
	declare -rg ZT_GAP="0x5"			# Gap zone

	# Example Usage:  if [[ ${target_type} == @(${ZT_NON_SEQ}) ]]; then...
	#                 if [[ ${target_type} != @(${ZT_WP}) ]]; then...

	declare -rg ZT_NON_SEQ="${ZT_CONV}|${ZT_SOBR}"	# Non-sequential zone types
	declare -rg ZT_SEQ="${ZT_SWR}|${ZT_SWP}"	# Sequential zone types
	declare -rg ZT_WP="${ZT_SEQ}|${ZT_SOBR}"	# Write Pointer zone

	declare -rg ZT_DISALLOW_WRITE_XTYPE="0x1|0x2|0x3|0x4|0x5"
							# Write across zone types disallowed XXX
	declare -rg ZT_DISALLOW_READ_XTYPE="0x1|0x2|0x3|0x4|0x5"
							# Read across zone types disallowed XXX

	declare -rg ZT_DISALLOW_WRITE_GT_WP="0x2|0x4"	# Write starting above WP disallowed
	declare -rg ZT_REQUIRE_WRITE_PHYSALIGN="0x2|0x4" # Write ending >= WP must be physical-block-aligned
	declare -rg ZT_RESTRICT_READ_GE_WP="0x2|0x4"	# Read ending above WP disallowed when !URSWRZ
	declare -rg ZT_RESTRICT_READ_INACTIVE="0x2|0x3|0x4"	# Read of INACTIVE zones disallowed when !URSWRZ

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
	declare -rg ZC_INACTIVE="0x5"			# INACTIVE zone condition
	declare -rg ZC_RDONLY="0xd"			# READ ONLY zone condition
	declare -rg ZC_FULL="0xe"			# FULL zone condition
	declare -rg ZC_OFFLINE="0xf"			# OFFLINE zone condition
	declare -rg ZC_NON_FULL="0x0|0x1|0x2|0x3|0x4"	# Non-FULL available zone conditions
	declare -rg ZC_AVAIL="${ZC_NON_FULL}|${ZC_FULL}" # available zone conditions

	# Expected Sense strings for ACTIVATE/QUERY status returns
	declare -rg ERR_ZA_SK="Unknown-sense-key 0x00"
	declare -rg ERR_ZA_ASC="Unknown-additional-sense-code-qualifier 0x00"

	# Zone Activation Status responses
	declare -rg ZA_STAT_NOT_INACTIVE="0x4001"	# Activating active zones
	declare -rg ZA_STAT_NOT_EMPTY="0x4002"		# Deactivating non-empty zones
	declare -rg ZA_STAT_REALM_ALIGN="0x4004"	# Realm alignment violation
	declare -rg ZA_STAT_MULTI_ZONE_TYPES="0x4008"	# Activation range crosses zone types
	declare -rg ZA_STAT_UNSUPP="0x4010"		# Activation of type unsupported
	declare -rg ZA_STAT_SECURITY="0x4040"		# Security prerequisite unmet
	declare -rg ZA_STAT_CROSS_DOMAINS="0x4028"	# Crossing domains also gets multiple zone types

	declare -rg ZBC_TEST_LIB_INIT=1
}

function zbc_test_lib_init()
{
	if [ -z "${ZBC_TEST_LIB_INIT}" ]; then
		_zbc_test_lib_init
	fi
}

zbc_test_lib_init

##### All code below is inside function declarations #####

function type_of_type_name()
{
	case "$1" in
	"conv")
		echo ${ZT_CONV}
		;;
	"seq")
		echo ${ZT_SWR}
		;;
	"seqp")
		echo ${ZT_SWP}
		;;
	"sobr")
		echo ${ZT_SOBR}
		;;
	"gap")
		echo ${ZT_GAP}
		;;
	* )
		zbc_test_fail_exit "Caller passed unsupported zone_type $1"
		;;
	esac
}

function type_name_of_type()
{
	case "$1" in
	${ZT_CONV})
		echo "conv"
		;;
	${ZT_SWR})
		echo "seq"
		;;
	${ZT_SWP})
		echo "seqp"
		;;
	${ZT_SOBR})
		echo "sobr"
		;;
	${ZT_GAP})
		echo "gap"
		;;
	* )
		zbc_test_fail_exit "Caller passed unsupported zone_type $1"
		;;
	esac
}

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

# Return 0 if $1 is a substring of $2; else return non-zero
function is_substr()
{
	local substr=$1
	local str=$2
	echo " $2" | grep -q -e "$1"
}

# $1 contains activation flags indicating whether we are requesting to use FSNOZ
function zbc_test_fsnoz_check_or_NA()
{
	if [ ${za_control} -eq 0 ]; then
		is_substr "--fsnoz" "$1"
		if [ $? -eq 0 ]; then
			zbc_test_print_not_applicable \
				"Device does not support setting FSNOZ"
		fi
	fi
}

function zbc_test_nozsrc_check_or_NA()
{
	if [ ${nozsrc} -eq 0 ]; then
		is_substr "--fsnoz" "$1"
		if [ $? -ne 0 ]; then
			zbc_test_print_not_applicable \
				"Device does not support NOZSRC/AUXSRC"
		fi
	fi
}

function zbc_test_cdb32_check_or_NA()
{
	if [ ! -z ${ZBC_TEST_DEV_ATA+x} ]; then
		is_substr "-32" "$1"
		if [ $? -eq 0 ]; then
			zbc_test_print_not_applicable \
				"Device does not support 32 byte CDBs"
		fi
	fi
}

# Sets ${seq_zone_type} and ${nr_avail_seq_zones}
function zbc_test_get_seq_type_nr()
{
	# If there are SWR zones we select that type; otherwise SWP.
	if [ ${seq_req_zone} -ne 0 ]; then
		seq_zone_type=${ZT_SWR}	# primary test using Sequential-write-required
	else
		# No SWR zones on the device -- use SWP
		seq_zone_type=${ZT_SWP}	# fallback test using Sequential-write-preferred
	fi

	# Get the number of available zones of the type we are using
	zbc_test_get_zone_info
	nr_avail_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${seq_zone_type}" \
				      | zbc_zone_filter_in_cond "${ZC_EMPTY}" | wc -l`
}

function zbc_test_have_max_open_or_NA()
{
	if [ ${max_open} -eq -1 ]; then
		zbc_test_print_not_applicable "Device does not report max_open"
	fi

	if [ ${max_open} -eq 0 ]; then
		if [ "${device_model}" != "Host-managed" ]; then
			zbc_test_print_not_applicable \
			    "Device is not Host-managed (max_open=0)"
		fi
		zbc_test_print_not_applicable \
		    "WARNING: Device reports max_open as zero"
	fi
}

# Finish a test-case script and specify its exit code
function zbc_test_case_post_process()
{
	echo "[TEST_TIME_END] `date -Ins`" >> ${log_file}

	zbc_test_check_failed
	local -i failed=$?

	# For observability, do not clean up temp files if the test failed.
	if [ ${failed} -eq 0 ]; then
		rm -f ${zone_info_file} ${zone_realm_info_file}
	else
		# If not in "batch" mode, leave device unmodified after failure
		if [ ! -z ${ZBC_TEST_BATCH_MODE} ]; then
			if [ ${ZBC_TEST_BATCH_MODE} -eq 0 ] ; then
				exit ${failed}
			fi
		elif [ ! -z ${batch_mode} ]; then
			if [ ${batch_mode} -eq 0 ] ; then
				exit ${failed}
			fi
		fi

	fi

	# Run the post-processing commands specified by the test script
	local -i i
	for (( i=0 ; i<${#_zbc_test_case_exit_handler[*]} ; i++ )) ; do
		${_zbc_test_case_exit_handler[i]}
	done

	if [[ ${SCRIPT_DEBUG} -ne 0 ]]; then
		# Induce an OZR counter check in the emulator
		${bin_path}/zbc_test_reset_zone ${device} -1
	fi

	exit ${failed}
}

# Scripts can call here to establish post-processing work for the running test case
function zbc_test_case_on_exit()
{
	_zbc_test_case_exit_handler+=("$*")
}

# For test script creation:
function zbc_test_init()
{
	if [ -n "${EXTENDED_TEST}" -a -z "${ZBC_RUN_EXTENDED_TESTS}" ]; then
		if [ -z "${NOT_EXTENDED_TEST}" ]; then
			exit 0
		fi
	fi

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

	test_case_id="${section_num}.${case_num}"
	test_case_desc="${desc}"

	if [ -z ${device} ]; then
		# Print description only
		echo "    ${test_case_id}: ${test_case_desc}"
		exit 0
	fi

	echo -n "    ${test_case_id}: ${test_case_desc}..."

	# Test log file
	log_file="${log_path}/${case_num}.log"
	rm -f ${log_file}

	# Zone info file
	zone_info_file="/tmp/zone_info.`basename ${device}`.${section_num}.${case_num}.log"
	rm -f ${zone_info_file}

	# Zone realm info file
	zone_realm_info_file="/tmp/zone_realm_info.`basename ${device}`.${section_num}.${case_num}.log"
	rm -f ${zone_realm_info_file}

	# Dump zone info file
	dump_zone_info_file="${log_path}/${case_num}_zone_info.log"

	# Dump zone realm info file
	dump_zone_realm_info_file="${log_path}/${case_num}_zone_realm_info.log"

	# List of commands to run when test case shell script exits
	_zbc_test_case_exit_handler=()
	trap zbc_test_case_post_process EXIT

	# Label the logfile
	echo "[LOG_FILE] ${log_file}" >> ${log_file}
	echo "[TEST_CASE] ${test_case_id}: ${test_case_desc}" >> ${log_file}
	echo "[TEST_TIME_START] `date -Ins`" >> ${log_file}
}

# Reset the DUT to factory conditions
function zbc_reset_test_device()
{
	local _IFS="${IFS}"
	local vendor=`${bin_path}/zbc_test_print_devinfo ${device} | grep -F '[VENDOR_ID]' | while IFS=$',\n' read a b; do echo $b; done`
	IFS="$_IFS"

	case "${vendor}" in
	*"TCMU DH-SMR"* )
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
		zbc_test_fail_exit "Can't reset test device"
	fi

	zbc_test_get_device_info

	local reason=""

	if [ ${maxact_control} -ne 0 ]; then
		# Allow the main ACTIVATE tests to run unhindered
		zbc_test_run ${bin_path}/zbc_test_dev_control -maxr unlimited ${device}
		if [ $? -ne 0 ]; then
			reason="because \'zbc_test_dev_control -maxr unlimited\' failed"
		else
			reason="even though \'zbc_test_dev_control -maxr unlimited\' returned success"
		fi
	else
		reason="because maxact_control=${maxact_control}"
	fi

	local _IFS="${IFS}"
	local max_act=`${bin_path}/zbc_test_print_devinfo ${device} | grep -F '[MAX_ACTIVATION]' | while IFS=$',\n' read a b; do echo $b; done`
	IFS="$_IFS"

	if [ -n "${max_act}" -a "${max_act}" != "unlimited" ]; then
		echo "WARNING: zbc_test_reset_device did not set (max_activation=${max_act}) to unlimited ${reason}" `_stacktrace`
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
	echo "## `date -Ins` Executing: ${_cmd}" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1
	${VALGRIND} ${_cmd} >> ${log_file} 2>&1

	return $?
}

function zbc_test_meta_run()
{
	local _cmd="$*"

	echo -e "\n### `date -Ins` Executing: ${_cmd}\n" 2>&1 | tee -a ${log_file} 2>&1

	${_cmd} 2>&1 | tee -a ${log_file} 2>&1
	local -i ret=${PIPESTATUS[0]}

	return ${ret}
}

# Get information functions

function zbc_check_string()
{
	if [ -z "$2" ]; then
		echo "$1"
		zbc_test_fail_exit
	fi
}

function zbc_test_get_device_info()
{
	if [ -z "${device}" ]; then
		zbc_test_fail_exit "device is not set -- call zbc_test_init before zbc_test_get_device_info"
	fi

	zbc_test_run ${bin_path}/zbc_test_print_devinfo -v ${device}
	local -i info_ret=$?
	if [ ${info_ret} -ne 0 ]; then
		zbc_test_fail_exit "Failed to get device info for ${device} (${info_ret})"
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

	local zdr_device_line=`cat ${log_file} | grep -F "[ZDR_DEVICE]"`
	set -- ${zdr_device_line}
	zdr_device=${2}
	zbc_check_string "Failed to get ZDR device support" ${zdr_device}

	local zone_realms_device_line=`cat ${log_file} | grep -F "[ZONE_REALMS_DEVICE]"`
	set -- ${zone_realms_device_line}
	zone_realms_device=${2}
	zbc_check_string "Failed to get Zone Realms device support" ${zone_realms_device}

	local zone_domains_device_line=`cat ${log_file} | grep -F "[ZONE_DOMAINS_DEVICE]"`
	set -- ${zone_domains_device_line}
	zone_domains_device=${2}
	zbc_check_string "Failed to get Zone Domains device support" ${zone_domains_device}

	if [ ${zdr_device} -ne 0 ]; then

		local ur_control_line=`cat ${log_file} | grep -F "[UR_CONTROL]"`
		set -- ${ur_control_line}
		ur_control=${2}
		zbc_check_string "Failed to get unrestricted read control" ${ur_control}

		local report_realms_line=`cat ${log_file} | grep -F "[REPORT_REALMS]"`
		set -- ${report_realms_line}
		report_realms=${2}
		zbc_check_string "Failed to get REPORT REALMS support" ${report_realms}

		local za_control_line=`cat ${log_file} | grep -F "[ZA_CONTROL]"`
		set -- ${za_control_line}
		za_control=${2}
		zbc_check_string "Failed to get zone activation control" ${za_control}

		local nozsrc_line=`cat ${log_file} | grep -F "[NOZSRC]"`
		set -- ${nozsrc_line}
		nozsrc=${2}
		zbc_check_string "Failed to get NOZSRC support" ${nozsrc}

		local maxact_control_line=`cat ${log_file} | grep -F "[MAXACT_CONTROL]"`
		set -- ${maxact_control_line}
		maxact_control=${2}
		zbc_check_string "Failed to get maximum activation realms control" ${maxact_control}

		local maxact_line=`cat ${log_file} | grep -F "[MAX_ACTIVATION]"`
		set -- ${maxact_line}
		max_act=${2}
		zbc_check_string "Failed to get maximum activation" ${max_act}

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

		local sobr_zone_line=`cat ${log_file} | grep -F "[SOBR_ZONE]"`
		set -- ${sobr_zone_line}
		sobr_zone=${2}
		zbc_check_string "Failed to get Sequential or Before Required zone support" ${sobr_zone}

		local conv_shifting_line=`cat ${log_file} | grep -F "[CONV_SHIFTING]"`
		set -- ${conv_shifting_line}
		conv_shifting=${2}
		zbc_check_string "Failed to get Conventional realm shifting boundaries flag" ${conv_shifting}

		local seq_shifting_line=`cat ${log_file} | grep -F "[SEQ_REQ_SHIFTING]"`
		set -- ${seq_shifting_line}
		seq_shifting=${2}
		zbc_check_string "Failed to get Sequential Write Required realm shifting boundaries flag" ${seq_shifting}

		local seqp_shifting_line=`cat ${log_file} | grep -F "[SEQ_PREF_SHIFTING]"`
		set -- ${seqp_shifting_line}
		seqp_shifting=${2}
		zbc_check_string "Failed to get Sequential Write Preferred realm shifting boundaries flag" ${seqp_shifting}

		local sobr_shifting_line=`cat ${log_file} | grep -F "[SOBR_SHIFTING]"`
		set -- ${sobr_shifting_line}
		sobr_shifting=${2}
		zbc_check_string "Failed to get Sequential or Before Required realm shifting boundaries flag" ${sobr_shifting}

	else
		ur_control=0
		report_realms=0
		za_control=0
		nozsrc=0
		maxact_control=0
		sobr_zone=0
		conv_zone=1
		if [ "${device_model}" = "Host-aware" ]; then
			seq_pref_zone=1
			seq_req_zone=0
		else
			seq_pref_zone=0
			seq_req_zone=1
		fi
	fi

	local nr_zones_lba_line=`cat ${log_file} | grep -F "[NR_ZONES]"`
	set -- ${nr_zones_lba_line}
	dev_nr_zones=${2}
	zbc_check_string "Failed to get number of zones" ${dev_nr_zones}

	local last_zone_lba_line=`cat ${log_file} | grep -F "[LAST_ZONE_LBA]"`
	set -- ${last_zone_lba_line}
	last_zone_lba=${2}
	zbc_check_string "Failed to get last zone start LBA" ${last_zone_lba}

	lblk_per_zone=$(( ${last_zone_lba} / ( ${dev_nr_zones} - 1 ) ))

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
	echo "## `date -Ins` Executing: ${_cmd} > ${zone_info_file} 2>&1" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1

	${VALGRIND} ${_cmd} > ${zone_info_file} 2>> ${log_file}
	last_ro="$ro"

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

function UNUSED_zbc_test_count_zones()
{
	nr_zones=`zbc_zones | wc -l`
}

function UNUSED_zbc_test_count_conv_zones()
{
	nr_conv_zones=`zbc_zones | zbc_zone_filter_in_type "${ZT_CONV}" | wc -l`
}

function UNUSED_zbc_test_count_seq_zones()
{
	nr_seq_zones=`zbc_zones | zbc_zone_filter_in_type "${ZT_SEQ}" | wc -l`
}

function UNUSED_zbc_test_count_inactive_zones()
{
	nr_inactive_zones=`zbc_zones | zbc_zone_filter_in_cond "${ZC_INACTIVE}" | wc -l`
}

# Set expected errors if zone is not available for write
function zbc_write_check_available()
{
	local _type="$1"
	local _cond="$2"

	if [ "${_cond}" = "${ZC_OFFLINE}" ]; then
		alt_expected_sk="Data-protect"
		alt_expected_asc="Zone-is-offline"
	elif [ "${_cond}" = "${ZC_RDONLY}" ]; then
		alt_expected_sk="Data-protect"
		alt_expected_asc="Zone-is-read-only"
	elif [ "${_cond}" = "${ZC_INACTIVE}" ]; then
		alt_expected_sk="Data-protect"
		alt_expected_asc="Zone-is-inactive"
	elif [ "${_type}" = "${ZT_GAP}" ]; then
		alt_expected_sk="Illegal-request"
		alt_expected_asc="Attempt-to-access-GAP-zone"
	else
		return 0
	fi

	return 1
}

# Set expected errors if zone is not available for read
function zbc_read_check_available()
{
	local _type="$1"
	local _cond="$2"

	if [ "${_cond}" = "${ZC_OFFLINE}" ]; then
		alt_expected_sk="Data-protect"
		alt_expected_asc="Zone-is-offline"
	elif [ "${_type}" = "${ZT_CONV}" ]; then
		return 0
	elif [ "${unrestricted_read}" -ne 0 ]; then
		return 0
	elif [ "${_cond}" = "${ZC_INACTIVE}" ]; then
		alt_expected_sk="Data-protect"
		alt_expected_asc="Zone-is-inactive"
	elif [ "${_type}" = "${ZT_GAP}" ]; then
		alt_expected_sk="Illegal-request"
		alt_expected_asc="Attempt-to-access-GAP-zone"
	else
		return 0
	fi

	return 1
}

# $1 is type of zones to open; $2 is number of zones to open
# It is expected that the requested number can be opened
function zbc_test_open_nr_zones()
{
	local _zone_cond="${ZC_EMPTY}"
	local _zone_type="${1}"
	local -i _open_num=${2}
	local -i _count=0
	if [ ${_open_num} -eq 0 ]; then
		return 0
	fi

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

	echo "FAIL: Opened ${_count} of ${_open_num} of type ${ZT_SWR} (max_open=${max_open})"
	zbc_test_dump_zone_info
	return 1
}

function zbc_test_iopen_nr_zones()
{
	local _zone_cond="${ZC_EMPTY}"
	local _zone_type="${1}"
	local -i _open_num=${2}
	local -i _count=0
	if [ ${_open_num} -eq 0 ]; then
		return 0
	fi

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
			echo "WARNING: Unexpected failure to write zone ${start_lba} after ${_count}/${_open_num} zones written"
			zbc_test_dump_zone_info
			return 1
		fi

		_count=${_count}+1

		if [ ${_count} -ge ${_open_num} ]; then
			return 0
		fi
	done

	echo "FAIL: Wrote ${_count} of ${_open_num} of type ${ZT_SWR} (max_open=${max_open})"
	zbc_test_dump_zone_info
	return 1
}

function zbc_test_close_nr_zones()
{
	local _zone_cond="${ZC_EMPTY}"
	local _zone_type="${1}"
	local -i _close_num=${2}
	local -i _count=0
	if [ ${_close_num} -eq 0 ]; then
		return 0
	fi

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

	echo "FAIL: Wrote/Closed ${_count} of ${_open_num} of type ${ZT_SWR} (max_open=${max_open})"
	zbc_test_dump_zone_info
	return 1
}

# This function expects always to find the requested slba
function zbc_test_get_target_zone_from_slba_or_fail()
{
	local start_lba=${1}
	if [ -z ${start_lba} ]; then
		zbc_test_fail_exit \
			"WARNING: zbc_test_get_target_zone_from_slba_or_fail called with empty start_lba argument"
	fi

	zbc_test_get_zone_info

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

	zbc_test_fail_exit "Cannot find slba=${slba} in ${zone_info_file}"
}

# This function expects always to find the requested slba
function zbc_test_get_target_zone_from_slba_or_fail_cached()
{
	local start_lba=${1}
	if [ -z ${start_lba} ]; then
		zbc_test_fail_exit \
			"WARNING: zbc_test_get_target_zone_from_slba_or_fail_cached called with empty start_lba argument"
	fi

	if [ ! -r "${zone_info_file}" ]; then
		zbc_test_get_zone_info
	elif [ "$last_ro" != "0" ]; then
		zbc_test_get_zone_info
	fi

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

	zbc_test_fail_exit "Cannot find slba=${slba} in ${zone_info_file}"
}

# Compatibility name is called from 81 scripts
function zbc_test_get_target_zone_from_slba()
{
	zbc_test_get_target_zone_from_slba_or_fail "$@"
}

# These _search_ functions look for a zone aleady in the condition

function zbc_test_search_target_zone_from_type_and_cond()
{
	local zone_type="${1}"
	local zone_cond="${2}"

	zbc_test_get_zone_info

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

function zbc_test_search_gap_zone_or_NA()
{
	local _zone_type="${ZT_GAP}"
	local _zone_cond="${ZC_NOT_WP}"

	zbc_test_search_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		zbc_test_print_not_applicable "No GAP zones"
	fi
}

function zbc_test_search_last_zone_vals_from_zone_type()
{
	local zone_type="${1}"

	zbc_test_get_zone_info

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

	zbc_test_search_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		return 1
	fi

	return 0
}

function zbc_test_search_zone_cond_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_search_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
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
		    "WARNING: Zone type ${_zone_type} is not a write-pointer zone type"
	fi

	zbc_test_search_zone_cond_or_NA "$@"
}

# Select a non-Sequential zone for testing and return info.
# Argument and return information are the same as zbc_test_search_zone_cond_or_NA.
function zbc_test_search_non_seq_zone_cond_or_NA()
{
	local _zone_type="${ZT_NON_SEQ}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_search_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	if [ $? -ne 0 ]; then
		zbc_test_print_not_applicable \
		    "No zone is of type ${_zone_type} and condition ${_zone_cond}"
	fi
}

# Select a Sequential zone for testing and return info.
# Argument and return information are the same as zbc_test_search_zone_cond_or_NA.
function zbc_test_search_seq_zone_cond()
{
	local _zone_type="${ZT_SEQ}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_search_target_zone_from_type_and_cond "${_zone_type}" "${_zone_cond}"
	return $?
}

function zbc_test_search_seq_zone_cond_or_NA()
{
	local _zone_type="${ZT_SEQ}"
	local _zone_cond="${1:-${ZC_AVAIL}}"

	zbc_test_search_seq_zone_cond "$@"
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
	local cand_type cand_cond cand_slba cand_size cand_ptr
	local cur_type cur_cond cur_slba cur_size cur_ptr
	local -i candidate=0
	local -i ret=1
	local -i i=0

	zbc_test_get_zone_info

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}" \
				| zbc_zone_filter_in_cond "${ZC_AVAIL}"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		cur_type=${3}
		cur_cond=${4}
		cur_slba=${5}
		cur_size=${6}
		cur_ptr=${7}

		IFS="$_IFS"

		if [[ $candidate == 0 ]]; then
			# Candidate first zone found
			cand_type=${cur_type}
			cand_cond=${cur_cond}
			cand_slba=${cur_slba}
			cand_size=${cur_size}
			cand_ptr=${cur_ptr}
			candidate=1
			i=1
		else
			if [[ ${cur_type} != ${cand_type} ]]; then
				candidate=0
				i=0
				continue
			fi

			i=$(($i + 1))
		fi

		if [ $i -ge $nz ]; then
			target_type=${cand_type}
			target_cond=${cand_cond}
			target_slba=${cand_slba}
			target_size=${cand_size}
			target_ptr=${cand_ptr}
			ret=0
			break
		fi
	done

	return $ret
}

function zbc_test_search_zone_pair()
{
	local zone_type="${1}"
	local zone1_cond=${2}
	local zone2_cond=${3}
	local cand_type cand_cond cand_slba cand_size cand_ptr
	local cur_type cur_cond cur_slba cur_size cur_ptr
	local -i candidate=0
	local -i ret=1

	zbc_test_get_zone_info

	for _line in `zbc_zones | zbc_zone_filter_in_type "${zone_type}"`; do

		local _IFS="${IFS}"
		IFS=$',\n'
		set -- ${_line}

		cur_type=${3}
		cur_cond=${4}
		cur_slba=${5}
		cur_size=${6}
		cur_ptr=${7}

		IFS="$_IFS"

		if [[ $candidate == 0 ]]; then
			# Make sure the first zone has the needed condition
			if [[ ${cur_cond} != @(${zone1_cond}) ]]; then
				continue
			fi

			# Candidate first zone found
			cand_type=${cur_type}
			cand_cond=${cur_cond}
			cand_slba=${cur_slba}
			cand_size=${cur_size}
			cand_ptr=${cur_ptr}
			candidate=1
			continue
		else
			# Make sure the second zone has the needed type/condition
			if [[ ${cur_type} != ${cand_type} ]]; then
				candidate=0
				continue
			fi
			if [[ ${cur_cond} != @(${zone2_cond}) ]]; then
				if [[ ${cur_cond} != @(${zone1_cond}) ]]; then
					candidate=0
					continue
				fi
				# No match with the second condition, but still a candidate
				cand_slba=${cur_slba}
				cand_size=${cur_size}
				cand_ptr=${cur_ptr}
				continue
			fi

			# Full match!
			target_type=${cand_type}
			target_cond=${cand_cond}
			target_slba=${cand_slba}
			target_size=${cand_size}
			target_ptr=${cand_ptr}
			ret=0
			break
		fi
	done

	return $ret
}

function zbc_test_search_zone_pair_or_NA()
{
	zbc_test_search_zone_pair "$@"
	if [ $? -ne 0 ]; then
		local zone_type="${1}"
		local zone1_cond=${2}
		local zone2_cond=${3}
		zbc_test_print_not_applicable \
		    "No available zone pair type=${zone_type} cond=${zone1_cond},${zone2_cond}"
	fi
}

# These _get_ functions set the zone(s) to the specified condition(s)

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
		"CLOSEDL")
			# CLOSE a zone with the first block written
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${lblk_per_pblk}
			zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_slba}
			;;
		"CLOSEDH")
			# CLOSE a zone with all but the last block written
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} $(( ${target_size} - ${lblk_per_pblk} ))
			zbc_test_run ${bin_path}/zbc_test_close_zone -v ${device} ${target_slba}
			;;
		"FULL")
			# FULL by writing the entire zone
			zbc_test_run ${bin_path}/zbc_test_reset_zone -v ${device} ${target_slba}
			zbc_test_run ${bin_path}/zbc_test_write_zone -v ${device} ${target_slba} ${target_size}
			;;
		"NOT_WP")
			if [[ ${ZT_CONV} != @(${zone_type}) ]]; then
				zbc_test_fail_exit "Caller requested condition ${cond} with zone type ${zone_type}"
			fi
			;;
		* )
			zbc_test_fail_exit "Caller requested unsupported condition ${cond}"
			;;
		esac

		shift
		zbc_test_get_target_zone_from_slba_or_fail $(( ${target_slba} + ${target_size} ))
	done

	# Update and return the info for the first zone of the tuple
	zbc_test_get_target_zone_from_slba_or_fail ${start_lba}
	return 0
}

function zbc_test_get_zones_cond_or_NA()
{
	local _zone_type="${test_zone_type:-${ZT_SEQ}}"

	zbc_test_get_zones_cond "${_zone_type}" "$@"
	if [ $? -ne 0 ]; then
	    if [ $# -gt 1 ]; then
		zbc_test_print_not_applicable \
		    "No available zone sequence of type ${_zone_type} and length $#"
	    else
		zbc_test_print_not_applicable \
		    "No available zone of type ${_zone_type}"
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

	zbc_test_get_zones_cond_or_NA "$@"
}

# Zone realm manipulation functions

function zbc_test_get_zone_realm_info()
{
	if [ ${report_realms} -eq 0 ]; then
		echo -n "(emulated REALMS)"
	fi

	if [ ${conv_zone} -ne 0 ]; then
		cmr_type="conv"
	elif [ ${sobr_zone} -ne 0 ]; then
		cmr_type="sobr"
	else
		zbc_test_print_not_applicable "Conventional zones are not supported by the device"
	fi

	if [ ${seq_req_zone} -ne 0 ]; then
		smr_type="seq"
	elif [ ${seq_pref_zone} -ne 0 ]; then
		smr_type="seqp"
	else
		zbc_test_print_not_applicable "Sequential zones are not supported by the device"
	fi

	local _cmd="${bin_path}/zbc_test_report_realms ${device}"
	echo "" >> ${log_file} 2>&1
	echo "## `date -Ins` Executing: ${_cmd} > ${zone_realm_info_file} 2>&1" >> ${log_file} 2>&1
	echo "" >> ${log_file} 2>&1

	${VALGRIND} ${_cmd} > ${zone_realm_info_file} 2>> ${log_file}

	_zbc_test_count_zone_realms
	_zbc_test_count_actv_as_conv_realms
	_zbc_test_count_actv_as_seq_realms

	return 0
}

function _zbc_test_count_zone_realms()
{
	nr_realms=`cat ${zone_realm_info_file} | grep "\[ZONE_REALM_INFO\]" | wc -l`

	if [ ${zdr_device} -ne 0 -a ${nr_realms} -eq 0 ]; then
		zbc_test_print_failed "Zone Domains device reports zero realm count"
		date -Ins
		exit 1
	fi
}

function _zbc_test_count_actv_as_conv_realms()
{
	local _IFS="${IFS}"
	nr_actv_as_conv_realms=`cat ${zone_realm_info_file} | while IFS=, read a b c d e f g h i j k; do echo $i; done | grep -c Y`
	IFS="$_IFS"
}

function _zbc_test_count_actv_as_seq_realms()
{
	local _IFS="${IFS}"
	nr_actv_as_seq_realms=`cat ${zone_realm_info_file} | while IFS=, read a b c d e f g h i j k; do echo $j; done | grep -c Y`
	IFS="$_IFS"
}

# Return non-zero if a realm found by zbc_test_search_* has zones R/O or offline
# Destroys target_zone information, so re-get the zone if you need it after calling here.
function zbc_test_is_found_realm_faulty()
{
	local _target_slba
	local _realm_start
	local _realm_len

	if [ "${test_faulty}" -eq 0 ]; then
		return 0
	fi

	zbc_test_get_zone_info

	for (( j=0 ; j<${realm_nr_domains} ; j++ )) ; do
		_realm_start=${realm_start_lba[j]}
		_realm_len=${realm_length[j]}
		_target_slba=${_realm_start}
		for (( i=0 ; i<${_realm_len} ; i++ )) ; do
			zbc_test_get_target_zone_from_slba_or_fail_cached ${_target_slba}
			if [ $? -ne 0 ]; then
				break
			fi
			if [ ${target_size} -eq 0 ]; then
				break
			fi
			if [[ ${target_cond} == @(${ZC_RDONLY}|${ZC_OFFLINE}) ]]; then
				return 1
			fi
			_target_slba=$(( ${target_slba} + ${target_size} ))
		done
	done

	return 0
}

function zbc_parse_realm_item()
{
	local _ifs="${IFS}"
	local -i _dom=${1}

	IFS=$':\n'
	set -- ${2}

	realm_dom_type[${_dom}]=${1}
	realm_start_lba[$_dom]=${2}
	realm_end_lba[$_dom]=${3}
	realm_length[$_dom]=${4}
	IFS="$_ifs"
	return 0
}

# Echo realm start LBA for a certain zone type. {1} is the type in textual form - conv|seq|seqp|sobr
# zbc_test_search_zone_realm_by_number() or zbc_test_search_realm_by_type_and_actv() must be called
# before attempting to call this one.
function zbc_realm_start()
{
	local -i _zt

	case "${1}" in
	"conv")
		_zt=$(( ${ZT_CONV} ))
		;;
	"seq")
		_zt=$(( ${ZT_SWR} ))
		;;
	"seqp")
		_zt=$(( ${ZT_SWP} ))
		;;
	"sobr")
		_zt=$(( ${ZT_SOBR} ))
		;;
	* )
		zbc_test_fail_exit "zbc_realm_start bad zone type arg=\"$1\""
		;;
	esac

	for (( i=0 ; i<${realm_nr_domains} ; i++ )) ; do
		if [[ ${realm_dom_type[i]} == $_zt ]]; then
			echo "${realm_start_lba[i]}"
			return 0
		fi
	done

	return 1
}

function zbc_realm_cmr_start()
{
	zbc_realm_start "${cmr_type}"
}

function zbc_realm_smr_start()
{
	zbc_realm_start "${smr_type}"
}

# Echo realm length in zones for a certain zone type. {1} is the type in textual form - conv|seq|seqp|sobr
# zbc_test_search_zone_realm_by_number() or zbc_test_search_realm_by_type_and_actv() must be called
# before attempting to call this one.
function zbc_realm_len()
{
	local -i _zt

	case "${1}" in
	"conv")
		_zt=$(( ${ZT_CONV} ))
		;;
	"seq")
		_zt=$(( ${ZT_SWR} ))
		;;
	"seqp")
		_zt=$(( ${ZT_SWP} ))
		;;
	"sobr")
		_zt=$(( ${ZT_SOBR} ))
		;;
	* )
		zbc_test_fail_exit "zbc_realm_len bad zone type arg=\"$1\""
		;;
	esac

	for (( i=0 ; i<${realm_nr_domains} ; i++ )) ; do
		if [[ ${realm_dom_type[i]} == $_zt ]]; then
			echo "${realm_length[i]}"
			return 0
		fi
	done

	return 1
}

function zbc_realm_cmr_len()
{
	zbc_realm_len "${cmr_type}"
}

function zbc_realm_smr_len()
{
	zbc_realm_len "${smr_type}"
}

function zbc_test_search_zone_realm_by_number()
{
	local realm_number=${1}

	if [ ${realm_number} -ge ${nr_realms} ]; then
		zbc_test_print_failed "realm=${realm_number} >= nr_realms=${nr_realms}"
		realm_nr_domains=0
		return 1
	fi

	# [ZONE_REALM_INFO],<num>,<domain>,<type>,<restr>,<allow_actv>,<allow_reset>,<actv_mask>,<actv_as_conv>,<actv_as_seq>,<nr_domains>;
	# 1                 2     3        4      5       6            7             8           9              10            11
	#
	# then <domain-spcific info>;...
	for _line in `cat ${zone_realm_info_file} | grep -E "\[ZONE_REALM_INFO\],(${realm_number}),.*,.*,.*,.*,.*,.*,.*,.*,.*"`; do

		local _IFS="${IFS}"
		local -i _dom=0

		IFS=$',\n'
		set -- ${_line}

		realm_num=$(( ${2} ))
		realm_domain=${3}
		realm_type=${4}
		realm_restrictions=${5}
		realm_allow_actv=${6}
		realm_allow_reset=${7}
		realm_actv_mask=${8}
		realm_actv_as_conv=${9}
		realm_actv_as_seq=${10}
		realm_nr_domains=${11}

		realm_dom_type=()
		realm_start_lba=()
		realm_end_lba=()
		realm_length=()

		IFS=$';\n'
		set -- ${_line}
		shift
		for item in $@; do
			zbc_parse_realm_item $_dom $item
			_dom=${_dom}+1
		done
		IFS="$_IFS"

		return 0

	done

	# Garbage attracts bugs, so clean it out
	realm_nr_domains=0
	realm_dom_type=()
	realm_start_lba=()
	realm_end_lba=()
	realm_length=()

	return 1
}

# Search for a realm containing LBA $1
function zbc_test_search_realm_by_lba()
{
	local _LBA=${1}

	if [ ${_LBA} -ge ${max_lba} ]; then
		zbc_test_print_failed "LBA=${_LBA} >= max_lba=${max_lba}"
		realm_nr_domains=0
		return 1
	fi

	# [ZONE_REALM_INFO],<num>,<domain>,<type>,<restr>,<allow_actv>,<allow_reset>,<actv_mask>,<actv_as_conv>,<actv_as_seq>,<nr_domains>;
	# 1                 2     3        4      5       6            7             8           9              10            11
	#
	# then <domain-spcific info>;...
	for _line in `cat ${zone_realm_info_file} | grep -E "\[ZONE_REALM_INFO\],.*,.*,.*,.*,.*,.*,.*,.*,.*,.*"`; do

		local _IFS="${IFS}"
		local -i _dom=0

		IFS=$',\n'
		set -- ${_line}

		realm_num=$(( ${2} ))
		realm_domain=${3}
		realm_type=${4}
		realm_restrictions=${5}
		realm_allow_actv=${6}
		realm_allow_reset=${7}
		realm_actv_mask=${8}
		realm_actv_as_conv=${9}
		realm_actv_as_seq=${10}
		realm_nr_domains=${11}

		realm_dom_type=()
		realm_start_lba=()
		realm_end_lba=()
		realm_length=()

		IFS=$';\n'
		set -- ${_line}
		shift
		for item in $@; do
			zbc_parse_realm_item $_dom $item
			_dom=${_dom}+1
		done
		IFS="$_IFS"

		for (( i=0 ; i<${realm_nr_domains} ; i++ )) ; do
			if [[ realm_start_lba[${i}] -le ${_LBA} &&
				    ${_LBA} -le realm_end_lba[${i}] ]]; then
				return 0
			fi
		done

	done

	# Garbage attracts bugs, so clean it out
	realm_nr_domains=0
	realm_dom_type=()
	realm_start_lba=()
	realm_end_lba=()
	realm_length=()

	return 1
}

# Return non-zero if realm $1 has zones R/O or offline
# Destroys target_zone and current realm information, so re-get if needed.
function zbc_test_is_realm_faulty()
{
	zbc_test_search_zone_realm_by_number $1
	zbc_test_is_found_realm_faulty
}

# $1 is realm type, $2 is can_activate_as
# Optional $3 = "NOFAULTY" specifies to skip faulty realms, and require two in a row.
# Destroys target_zone information, so re-get the zone if you need it after calling here.
function zbc_test_search_realm_by_type_and_actv()
{
	local realm_search_type=${1}
	local _NOFAULTY="$3"
	local actv

	case "${2}" in
	"conv")
		actv="Y,.*"
		;;
	"noconv")
		actv="N,.*"
		;;
	"seq")
		actv=".*,Y"
		;;
	"noseq")
		actv=".*,N"
		;;
	"both")
		actv="Y,Y"
		;;
	"none")
		actv="N,N"
		;;
	* )
		zbc_test_fail_exit "zbc_test_search_realm_by_type_and_actv bad can_activate_as arg=\"$2\""
		;;
	esac

	# [ZONE_REALM_INFO],<num>,<domain>,<type>,<restr>,<allow_actv>,<allow_reset>,<actv_mask>,<actv_as_conv>,<actv_as_seq>,<nr_domains>;
	# 1                 2     3        4      5       6            7             8           9              10            11
	#
	# then <domain-spcific info>;...
	for _line in `cat ${zone_realm_info_file} | grep -E "\[ZONE_REALM_INFO\],.*,.*,(${realm_search_type}),.*,.*,.*,0x.*,${actv},.*"`; do

		local _IFS="${IFS}"
		local -i _dom=0

		IFS=$',\n'
		set -- ${_line}

		realm_num=$(( ${2} ))
		realm_domain=${3}
		realm_type=${4}
		realm_restrictions=${5}
		realm_allow_actv=${6}
		realm_allow_reset=${7}
		realm_actv_mask=${8}
		realm_actv_as_conv=${9}
		realm_actv_as_seq=${10}
		realm_nr_domains=${11}

		realm_dom_type=()
		realm_start_lba=()
		realm_end_lba=()
		realm_length=()

		IFS=$';\n'
		set -- ${_line}
		shift
		for item in $@; do
			zbc_parse_realm_item $_dom $item
			_dom=${_dom}+1
		done
		IFS="$_IFS"

		if [ "${realm_allow_actv}" != "Y" ]; then
			continue
		fi

		if [ "${_NOFAULTY}" != "NOFAULTY" ]; then
			return 0
		fi

		# NOFAULTY:
		# Ensure the returned realm is OK for write testing, etc
		zbc_test_is_found_realm_faulty
		if [ $? -ne 0 ]; then
			continue
		fi

		# Ensure two contiguous non-faulty realms needed by some tests.
		# The realms must both have the requested type and actv_as_.

		if [ $(( ${realm_num} + 1 )) -ge ${nr_realms} ]; then
			continue	# second realm number out of range
		fi

		local -i found_realm_num=${realm_num}
		local found_realm_type="${realm_type}"
		local found_realm_actv_conv="${realm_actv_as_conv}"
		local found_realm_actv_seq="${realm_actv_as_seq}"

		zbc_test_is_realm_faulty $(( ${realm_num} + 1 ))
		if [ $? -ne 0 ]; then
			continue	# second realm is faulty
		fi

		if [ "${realm_type}" != "${found_realm_type}" ]; then
			continue	# second realm is different type
		fi

		if [ "${realm_actv_as_seq}" != "${found_realm_actv_seq}" ]; then
			continue	# second realm mismatches seq_actv
		fi

		if [ "${realm_actv_as_conv}" != "${found_realm_actv_conv}" ]; then
			continue	# second realm mismatches conv_actv
		fi

		# Reset the found realm to the first of the pair
		zbc_test_search_zone_realm_by_number ${found_realm_num}
		return 0

	done

	# Garbage attracts bugs, so clean it out
	realm_nr_domains=0
	realm_dom_type=()
	realm_start_lba=()
	realm_end_lba=()
	realm_length=()

	return 1
}

function zbc_test_search_realm_by_type_and_actv_or_NA()
{
	zbc_test_search_realm_by_type_and_actv "$@"
	if [[ $? -ne 0 ]]; then
		zbc_test_print_not_applicable \
		    "No realms of type $1 and activatable as $2 $3"
	fi
}

function zbc_test_realm_boundaries_not_shifting_or_NA()
{
	local flg="$1_shifting"

	if [ ${!flg} -ne 0 ]; then
		zbc_test_print_not_applicable \
		    "Shifting realms of type $1 are not supported for this operation"
	fi
}

function zbc_test_calc_nr_realm_zones()
{
	local _realm_num=${1}
	local -i _nr_realms=${2}
	local _actv_as_conv
	local _actv_as_seq
	local -i _nr_domains
	nr_conv_zones=0
	nr_seq_zones=0

	# [ZONE_REALM_INFO],<num>,<domain>,<type>,<restr>,<allow_actv>,<allow_reset>,<actv_mask>,<actv_as_conv>,<actv_as_seq>,<nr_domains>;
	# 1                 2     3        4      5       6            7             8           9              10            11
	#
	# then <domain-spcific info>;...
	for _line in `cat ${zone_realm_info_file} | grep "\[ZONE_REALM_INFO\]"`; do

		local _IFS="${IFS}"
		local -i _dom

		IFS=$',\n'
		set -- ${_line}

		if [[ $(( ${2} )) -ge $(( ${_realm_num} )) ]]; then

			_actv_as_conv=${9}
			_actv_as_seq=${10}
			_nr_domains=${11}

			IFS=$';\n'
			set -- ${_line}
			shift
			_dom=0
			for item in $@; do
				zbc_parse_realm_item $_dom $item
				_dom=${_dom}+1
			done

			if [ "${_actv_as_conv}" == "Y" ]; then
				for (( i=0; i<_nr_domains; i++ )); do
					if [[ ${realm_dom_type[i]} == $(( ${ZT_CONV} )) || \
					      ${realm_dom_type[i]} == $(( ${ZT_SOBR} )) ]]; then
						nr_conv_zones=$(( ${nr_conv_zones} + ${realm_length[i]} ))
						break
					fi
				done
			fi

			if [ "${_actv_as_seq}" == "Y" ]; then
				for (( i=0; i<_nr_domains; i++ )); do
					if [[ ${realm_dom_type[i]} == $(( ${ZT_SWR} )) || \
					      ${realm_dom_type[i]} == $(( ${ZT_SWP} )) ]]; then
						nr_seq_zones=$(( ${nr_seq_zones} + ${realm_length[i]} ))
						break
					fi
				done
			fi

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

	local sk_line=`tac ${log_file} | grep -m 1 -F "[SENSE_KEY]"`
	set -- ${sk_line}
	sk=${2}

	local asc_line=`tac ${log_file} | grep -m 1 -F "[ASC_ASCQ]"`
	set -- ${asc_line}
	asc=${2}

	local err_za_line=`tac ${log_file} | grep -m 1 -F "[ERR_ZA]"`
	set -- ${err_za_line}
	err_za=${2}

	local err_cbf_line=`tac ${log_file} | grep -m 1 -F "[ERR_CBF]"`
	set -- ${err_cbf_line}
	err_cbf=${2}

	IFS="$_IFS"
}

function zbc_test_reset_err()
{
	echo "Resetting log error:" >> ${log_file}
	echo "[TEST][ERROR][SENSE_KEY],," >> ${log_file}
	echo "[TEST][ERROR][ASC_ASCQ],," >> ${log_file}
	echo "[TEST][ERROR][ERR_ZA],," >> ${log_file}
	echo "[TEST][ERROR][ERR_CBF],," >> ${log_file}
}

function zbc_test_print_res()
{
	local width=`tput cols`

	width=$(( ${width} - 9 ))
	if [ ${width} -gt 108 ]; then
		width=108
	fi

	# Print name of logfile for failing tests
	if [ ${2:0:1} = "F" ]; then
		local _L=" ${log_file}"
	fi

	echo "" >> ${log_file} 2>&1
	echo "TESTRESULT==$2" >> ${log_file} 2>&1
	echo -e "\r\e[${width}C[$1$2${end}]${_L}"
}

function zbc_test_print_passed()
{
	if [ $# -gt 0 -a -n "${ZBC_TEST_PASS_DETAIL}" ]; then
		zbc_test_print_res "${green}" "Passed $*"
	else
		zbc_test_print_res "${green}" "Passed"
	fi

	if [[ ${SCRIPT_DEBUG} -ne 0 ]]; then
	    zbc_test_dump_info
	fi
}

function zbc_test_print_passed_lib()
{
	if [ $# -gt 0 -a -n "${ZBC_TEST_PASS_DETAIL}" ]; then
		zbc_test_print_res "${green}" "Passed (libzbc) $*"
	else
		zbc_test_print_res "${green}" "Passed (libzbc)"
	fi

	if [[ ${SCRIPT_DEBUG} -ne 0 ]]; then
	    zbc_test_dump_info
	fi
}

function zbc_test_print_not_applicable()
{
	zbc_test_print_res "" "N/A $*"
	if [[ ${SCRIPT_DEBUG} -ne 0 ]]; then
	    zbc_test_dump_info
	fi
	exit 0
}

function zbc_test_print_failed()
{
	if [ $# -gt 0 ]; then
		zbc_test_print_res "${red}" "Failed $*"
	else
		zbc_test_print_res "${red}" "Failed"
	fi
}

function zbc_test_print_failed_sk()
{
	zbc_test_print_failed "$@"

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
}

function zbc_test_check_err()
{
	if [ -n "${ZBC_ACCEPT_ANY_FAIL}" -a -n "${expected_err_za}" ]; then
		if [ -n "${sk}" ]; then
			expected_err_za="${err_za}"
			err_cbf="${expected_err_cbf}"
			expected_sk="${sk}"
			expected_asc="${asc}"
		fi
	fi

	if [ -n "${expected_err_za}" -a -z "${expected_err_cbf}" ] ; then
		# Our caller expects ERR_ZA, but specified no expected CBF -- assume zero
		local expected_err_cbf=0	# expect (CBF == 0)
	fi

	if [ "${sk}" = "${expected_sk}" -a "${asc}" = "${expected_asc}" \
			-a "${err_za}" = "${expected_err_za}" -a "${err_cbf}" = "${expected_err_cbf}" ]; then
		zbc_test_print_passed "$*"
	else
		zbc_test_print_failed_sk "$*"
	fi
}

function zbc_test_check_sk_ascq()
{
	if [ -n "${ZBC_ACCEPT_ANY_FAIL}" -a -n "${expected_sk}" ]; then
		if [ -n "${sk}" ]; then
			zbc_test_print_passed "$*"
			return
		fi
	fi

	if [ "${sk}" = "${expected_sk}" -a "${asc}" = "${expected_asc}" ]; then
		zbc_test_print_passed "$*"
		return
	fi

	if [[ -n ${alt_expected_sk+x} && "${sk}" == "${alt_expected_sk}" &&
	      -n ${alt_expected_asc+x} && "${asc}" == "${alt_expected_asc}" ]]; then
		zbc_test_print_passed "$*"
		return
	fi

	zbc_test_print_failed_sk "$*"
}

function zbc_test_check_no_sk_ascq()
{
	local expected_sk=""
	local expected_asc=""
	if [ -z "${sk}" -a -z "${asc}" ]; then
		zbc_test_print_passed "$*"
	else
		zbc_test_print_failed_sk "$*"
	fi
}

function zbc_test_fail_if_sk_ascq()
{
	zbc_test_get_sk_ascq
	if [ -n "${sk}" -o -n "${asc}" ]; then
		local expected_sk=""
		local expected_asc=""
		zbc_test_print_failed_sk "$*"
		return 1
	fi
	return 0
}

function zbc_test_fail_exit_if_sk_ascq()
{
	zbc_test_fail_if_sk_ascq "$*"
	if [[ $? -ne 0 ]]; then
		exit 1
	fi
	return 0
}

function zbc_test_fail_exit()
{
	zbc_test_fail_exit_if_sk_ascq "$*"
	zbc_test_print_failed "$* (from `_stacktrace`)"
	exit 1
}

function zbc_test_print_failed_zc()
{
	zbc_test_print_failed "$@"

	echo "=> Expected zone condition ${expected_cond}, Got ${target_cond}" >> ${log_file} 2>&1
	echo "            => Expected zone condition ${expected_cond}"
	echo "                    Got zone condition ${target_cond}"
}

# zbc_test_check_wp_eq expected_wp err_msg
function zbc_test_check_wp_eq()
{
	local -i expected_wp=$1
	shift
	if [ ${target_ptr} -ne ${expected_wp} ]; then
		zbc_test_print_failed "(WP=${target_ptr}) != (expected=${expected_wp}); $*"
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
		zbc_test_print_failed "(WP=${target_ptr}) != (expected=${expected_wp}); $*"
		zbc_test_print_failed "(WP=${target_ptr}) not within [${wp_min}, ${wp_max}]; $*"
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
		zbc_test_print_passed "$*"
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
		zbc_test_print_failed "(WP=${target_ptr}) != (expected=${expected_wp}); $*"
	else
		zbc_test_print_passed "$*"
	fi
}

# Check for expected_sk/expected_asc and expected_cond
function zbc_test_check_sk_ascq_zone_cond()
{
	if [ -n "${ZBC_ACCEPT_ANY_FAIL}" -a -n "${expected_sk}" ]; then
		if [ -n "${sk}" ]; then
			expected_sk="${sk}"
			expected_asc="${asc}"
		fi
	fi

	if [ "${sk}" != "${expected_sk}" -o "${asc}" != "${expected_asc}" ]; then
		zbc_test_print_failed_sk "$*"
	elif [ "${target_cond}" != "${expected_cond}" ]; then
		zbc_test_print_failed_zc "$*"
	else
		zbc_test_print_passed "$*"
	fi
}

# Check for expected_sk/expected_asc, expected_cond, and expected_wp
function zbc_test_check_sk_ascq_zone_cond_wp()
{
	local -i expected_wp=$1
	shift

	if [ -n "${ZBC_ACCEPT_ANY_FAIL}" -a -n "${expected_sk}" ]; then
		if [ -n "${sk}" ]; then
			expected_sk="${sk}"
			expected_asc="${asc}"
		fi
	fi

	if [ "${sk}" != "${expected_sk}" -o "${asc}" != "${expected_asc}" ]; then
		zbc_test_print_failed_sk "$*"
	elif [ "${target_cond}" != "${expected_cond}" ]; then
		zbc_test_print_failed_zc "$*"
	elif [ ${target_ptr} -ne ${expected_wp} ]; then
		zbc_test_print_failed "(WP=${target_ptr}) != (expected=${expected_wp}); $*"
	else
		zbc_test_print_passed "$*"
	fi
}

function zbc_test_dump_zone_info()
{
	zbc_report_zones ${device} > ${dump_zone_info_file}
}

function zbc_test_dump_zone_realm_info()
{
	zbc_report_domains ${device} > ${dump_zone_realm_info_file}
	zbc_report_realms ${device} >> ${dump_zone_realm_info_file}
}

function zbc_test_dump_info()
{
	zbc_test_dump_zone_info
	if [ "${zdr_device}" -ne 0 ]; then
		zbc_test_dump_zone_realm_info
	fi
}

# Dump info files after a failed test -- returns 1 if test failed
function zbc_test_check_failed()
{
	failed=`cat ${log_file} | grep -m 1 "TESTRESULT==Failed"`
	if [[ ! -z "${failed}" ]]; then
		zbc_test_dump_info
		return 1
	fi

	return 0
}
