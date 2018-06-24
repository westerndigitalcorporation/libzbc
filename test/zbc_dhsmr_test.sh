#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
#

#-list output format
#
#Section 00 - command completion tests
#   00.001 - xxxx
#   00.002 - xxxx
#   ...
#01 - Sense key, sense code tests
#   01.001 - xxxx
#   01.002 - xxxx
#...

progname=$0

function zbc_print_usage()
{

	echo "Usage: $0 [Options] [Device path]"
	echo "Options"
	echo "  -h | --help              : Print this usage"
	echo "  -l | --list              : List all test cases"
	echo "  -b | --batch             : Use batch mode (do not stop on failed tests)"
	echo "  -q | --quick             : Test only one activation per mutation and fewer mutations"
	echo "  -e | --exec <test number>: Execute only the specified test."
	echo "                             This option may be repeated multiple times"
	echo "                             to execute multiple tests in one run."
	echo "  -s | --skip <test number>: Skip execution of the specified test."
	echo "                             This option may be repeated multiple times"
	echo "                             to skip the execution of multiple tests."
	echo "  -a | --ata               : Force the use of the ATA backend driver for ZAC devices"
	echo "  -f | --format            : Force formatting of the test device. By default, only"
	echo "                             TCMU emulated device is formatted."
	echo "  -n | --noformat          : Skip formatting of the test device."
	echo "Test numbers must be in the form \"<section number>.<case number>\"."
	echo "The device path can be omitted with the -h and -l options."
	echo "If -e and -s are not used, all defined test cases are executed."

	exit 1
}

if [ $# -lt 1 ]; then
    zbc_print_usage
fi

# Check credentials
if [ $(id -u) -ne 0 ]; then
	echo "Only root can run tests."
	exit 1
fi

# Check tests subdirs
cd `dirname $0`

# Test programs directory
ZBC_TEST_BIN_PATH=programs
if [ ! -d ${ZBC_TEST_BIN_PATH} ]; then
    echo "Test program directory ${ZBC_TEST_BIN_PATH} does not exist"
    exit 1
fi
bin_path=${ZBC_TEST_BIN_PATH}

# Binary check
test_progs=( \
    zbc_test_print_devinfo \
    zbc_test_report_zones \
    zbc_test_reset_zone \
    zbc_test_open_zone \
    zbc_test_close_zone \
    zbc_test_finish_zone \
    zbc_test_read_zone \
    zbc_test_write_zone \
    zbc_test_domain_report \
    zbc_test_zone_activate \
    zbc_test_dev_control \
)

for p in ${test_progs[@]}; do
	path=${ZBC_TEST_BIN_PATH}/${p}
	if [ ! -e ${path} ]; then
		echo "Test program ${p} not found in directory ${ZBC_TEST_BIN_PATH}"
		echo "Re-configure with option \"--with-test\" and recompile"
		exit 1
	fi
done

ZBC_TEST_SCR_PATH=scripts
if [ ! -d ${ZBC_TEST_SCR_PATH} ]; then
    echo "Test script directory ${ZBC_TEST_SCR_PATH} does not exist"
    exit 1
fi

# Handle arguments
argv=("$@")
argc=$#
argimax=$((argc-1))

exec_list=()
_eexec_list=()		# -ee args passed down to ZA meta-children as -e
_cexec_list=()		# -e args passed down to HA/HM meta-children as -e
skip_list=()		# The skip_list argument may be a regular expression
_cskip_list=()		# meta-child inherits the skip_list
print_list=0
export batch_mode=0	# If set, do not stop on first failed test script
force_ata=0
format_dut=0
skip_format_dut=0

# Store argument
for (( i=0; i<${argc}; i++ )); do

	case ${argv[$i]} in
	-h | --help )
		zbc_print_usage
		;;
	-l | --list )
		print_list=1
		break
		;;
	-b | --batch)
		batch_mode=1
		;;
	--valgrind )
		export VALGRIND="valgrind --num-callers=24 --read-var-info=yes --leak-check=full --show-leak-kinds=all --track-origins=yes"
		;;
	-e | --exec )
		i=$((i+1))
		exec_list+=(${argv[$i]})
		_cexec_list+=" -e "
		_cexec_list+="${argv[$i]}"
		;;
	-ee )
		i=$((i+1))
		_eexec_list+=" -e "
		_eexec_list+="${argv[$i]}"
		;;
	-s | --skip )
		i=$((i+1))
		skip_list+=("${argv[$i]}")
		_cskip_list+=" -s "
		_cskip_list+="${argv[$i]}"
		;;
	-a | --ata )
		force_ata=1
		;;
	-f | --format )
		format_dut=1
		;;
	-n | --noformat )
		skip_format_dut=1
		;;
	-q | --quick )
		ZBC_MUTATIONS=""
		ZA_MUTATIONS="ZA_1CMR_BOT ZA_1CMR_BOT_SWP"
		WPC_MUTATIONS="ZA_WPC"
		skip_list+=("04.010")
		skip_list+=("04.030")
		skip_list+=("04.040")
		skip_urswrz_n=1
		;;
	-* )
		echo "Unknown option \"${argv[$i]}\""
		zbc_print_usage
		;;
	* )
		if [ "${device}" != "" -o $i -ne ${argimax} ]; then
			zbc_print_usage
		fi
		device="${argv[$i]}"
		;;
	esac

done

if [ ${force_ata} -ne 0 ]; then
	export ZBC_TEST_FORCE_ATA="ATA"
else
	unset ZBC_TEST_FORCE_ATA
fi

export eexec_list="${_eexec_list[*]}"
export cexec_list="${_cexec_list[*]}"
export cskip_list="${_cskip_list[*]}"

if [ ${print_list} -ne 0 ]; then
	exec_list=()
	skip_list=()
	device=""
fi

# Check device path
if [ ! -z "${device}" ]; then
	if [ ! -e ${device} ]; then
		echo "Device \"${device}\" not found"
		exit 1
	fi
	dev_name=`basename ${device}`
	if [[ "${dev_name}" =~ "sd" ]]; then
		sg_dev=$(ls /sys/block/${dev_name}/device/scsi_generic)
		if [ ! -z "$sg_dev" ]; then
			echo "Using sg device /dev/${sg_dev} to perform tests for ${device}"
			dev_name=${sg_dev}
			device="/dev/${sg_dev}"
		fi
	fi
fi

# Build run list from section number arguments
function get_exec_list()
{
	local secnum casenum file
	for secnum in "$@" ; do
		for file in ${ZBC_TEST_SCR_PATH}/${secnum}*/*.sh; do
			local _IFS="${IFS}"
			IFS='.'
			set -- ${file}
			casenum=`basename ${1}`
			IFS="$_IFS"
			exec_list+=(${secnum}.${casenum})
		done
	done
}

function get_section_num()
{
	local testnum=$1
	local _IFS="${IFS}"
	IFS='.'
	set -- ${testnum}
	echo "${1}"
	IFS="$_IFS"
}

function get_case_num()
{
	local testnum=$1
	local _IFS="${IFS}"
	IFS='.'
	set -- ${testnum}
	echo "${2}"
	IFS="$_IFS"
}

# Prepare run lists based on arguments which are section numbers
function prepare_lists()
{
	if [ ${#exec_list[@]} -eq 0 ]; then
		get_exec_list "$@"
	fi

	_IFS="$IFS"
	IFS="\n"
	if [ ${#exec_list[@]} -gt 0 ]; then
		exec_list=(`for e in ${exec_list[@]}; do echo "${e}"; done | sort -u`)
	fi
	if [ ${#skip_list[@]} -gt 0 ]; then
		skip_list=(`for s in ${skip_list[@]}; do echo "${s}"; done | sort -u`)
	fi
	IFS="$_IFS"

	# Subtract skip list from exec list. At the same time,
	# extract the section list.
	run_list=()
	section_list=()
	for e in ${exec_list[@]}; do

		run=1

		for s in ${skip_list[@]}; do
			if [[ "${e}" == @(${s}) ]]; then
				run=0
				break
			fi
		done

		if [ ${run} -eq 0 ]; then
			continue
		fi

		run_list+=(${e})
		section_num=`get_section_num ${e}`
		section_list+=(${section_num})

	done

	section_list=(`for s in ${section_list[@]}; do echo "${s}"; done | sort -u`)
}

# Run test cases of a section
function zbc_run_section()
{
	local ret=0
	local section_num="$1"
	local section_name="$2"

	local section_path=`find ${ZBC_TEST_SCR_PATH} -type d -name "${section_num}*" -print`
	if [ -z "${section_path}" ]; then
		echo "Test script directory ${section_path} does not exist"
		exit 1
	fi

	local log_path=${ZBC_TEST_LOG_PATH}/${section_num}
	mkdir -p ${log_path}

	if [ ${print_list} -ne 0 ]; then
		# Printing test cases only
		echo "Section ${section} - ${section_name} tests"
	else
		# Init: Close and reset all zones
		${ZBC_TEST_BIN_PATH}/zbc_test_close_zone ${device} -1
		${ZBC_TEST_BIN_PATH}/zbc_test_reset_zone ${device} -1
		echo "Executing section ${section} - ${section_name} tests..."
	fi

	# Execute test cases for this section
	local t s c res
	for t in ${run_list[@]}; do

		s=`get_section_num ${t}`
		if [ "${s}" != "${section_num}" ]; then
			continue
		fi

		c=`get_case_num ${t}`
		./${section_path}/${c}.sh ${ZBC_TEST_BIN_PATH} ${log_path} ${section_num} ${device}
		ret=$(( ${ret} | $? ))

		if [ ${print_list} -ne 1 ]; then
			res="`cat ${log_path}/${c}.log | grep TESTRESULT`"
			if [ ${ret} -ne 0 -o "${res}" = "TESTRESULT==Failed" ]; then
				ret=1
				if [ ${batch_mode} -eq 0 ]; then
					break
				fi
			fi
		fi

	done

	return ${ret}
}

. scripts/zbc_test_lib.sh	# for zbc_test_reset_device

function reset_device()
{
	# Reset the device if needed
	if [ ${skip_format_dut} -eq 0 ]; then
		zbc_test_reset_device
	fi
}

# Run tests
function zbc_run_config()
{
    local section_name
    local rc=0

    for section in "$@" ; do

	case "${section}" in
	"00")
		section_name="command completion"
		;;
	"01")
		section_name="sense key, sense code"
		;;
	"02")
		section_name="zone state machine"
		;;
	"03")
		section_name="DH-SMR command"
		;;
	"04")
		section_name="DH-SMR device ZBC"
		;;
	"05")
		section_name="DH-SMR WPC zone"
		;;
	"08")
		section_name="SCSI-only"
		;;
	"09")
		section_name="site-local (unpublished)"
		;;
	* )
		echo "Unknown test section ${section}"
		exit 1
		;;
	esac

	zbc_run_section "${section}" "${section_name}"
	rc=$(( ${rc} | $? ))
	if [ ${rc} -ne 0 -a ${batch_mode} -eq 0 ]; then
		break
	fi

    done

    return ${rc}
}

function set_logfile()
{
    if [ -z "$1" ]; then
        ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH_BASE}
    else
        ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH_BASE}/$1
    fi
    local log_path=${ZBC_TEST_LOG_PATH}
    mkdir -p ${log_path}
    log_file="${log_path}/zbc_dhsmr_test.log"
    rm -f ${log_file}
}

# $1 is mutation name
function zbc_run_mutation()
{
    zbc_test_get_device_info

    if [ "${ur_control}" == 0 ]; then
        echo -e "\n###### Device doesn't support unrestricted reads control"
        if [ "${unrestricted_read}" == 0 ]; then
            echo "###### Running the dhsmr test suite with URSWRZ disabled"
	    set_logfile $1/urswrz_n
        else
            echo "###### Running the dhsmr test suite with URSWRZ enabled"
	    set_logfile $1/urswrz_y
        fi
        reset_device
        zbc_run_config ${section_list[@]}
    else
        echo -e "\n###### Run the dhsmr test suite with URSWRZ enabled"
	set_logfile $1/urswrz_y
        reset_device
        zbc_test_run ${ZBC_TEST_BIN_PATH}/zbc_test_dev_control -q -ur y ${device}
        zbc_run_config ${section_list[@]}

	if [ -z ${skip_urswrz_n} ]; then
	    echo -e "\n###### Run the dhsmr test suite with URSWRZ disabled"
	    set_logfile $1/urswrz_n
	    reset_device
	    zbc_test_run ${ZBC_TEST_BIN_PATH}/zbc_test_dev_control -q -ur n ${device}
	    zbc_run_config ${section_list[@]}
	fi
    fi
}

function zbc_run_gamut()
{
    uname -a		# Record machine and kernel info

    zbc_test_get_device_info

    if [ ${mutations} == 0 ]; then
        echo -e "\n\n######### `date` Device doesn't support mutation"
        zbc_run_mutation ""
	return
    fi

    if [ -n "${ZA_MUTATIONS}" -o -n "${WPC_MUTATIONS}" ]; then
      for m in ${ZA_MUTATIONS} ${WPC_MUTATIONS} ; do
	echo -e "\n\n######### `date` Run the dhsmr test suite under mutation ${m}"
	set_logfile ${m}
	zbc_test_run zbc_dev_control -v -mu ${m} ${device}
	if [ $? -ne 0 ]; then
	    echo "Mutation of ${device} to ${m} failed"
	    continue
	fi
        reset_device
	zbc_run_mutation "${m}"
      done
    fi

    if [ -n "${ZBC_MUTATIONS}" ]; then
      for m in ${ZBC_MUTATIONS} ; do
	echo -e "\n\n######### `date` Run the zbc test suite under mutation ${m}"
	set_logfile ${m}
	zbc_test_run zbc_dev_control -v -mu ${m} ${device}
	if [ $? -ne 0 ]; then
	    echo "Mutation of ${device} to ${m} failed"
	    continue
	fi
        reset_device

	local arg_b=""
	if [ ${batch_mode} -ne 0 ] ; then
	    arg_b="-b"
	fi

	local arg_a=""
	if [ ${force_ata} -ne 0 ]; then
		arg_a="-a"
	fi

	(   # subshell protects outer shell variables from the changes made here
	    ZBC_TEST_LOG_PATH_BASE=${ZBC_TEST_LOG_PATH_BASE}/${m}
    	    ZBC_TEST_SECTION_LIST="00 01 02"	# for ZBC meta-children
	    zbc_test_meta_run ./zbc_dhsmr_test.sh ${arg_a} ${arg_b} -n ${cexec_list} ${cskip_list} ${device}
	)
      done
    fi

    echo -e "\n\n######### `date` Last test completed"	# for the datestamp

    set_logfile "fini"

    # When done, set the device back to default
    zbc_test_run zbc_dev_control -v -mu ZA_1CMR_BOT ${device}
    if [ $? -ne 0 ]; then
	echo "Mutation of ${device} to ${m} failed"
    fi
    reset_device
    zbc_test_run ${ZBC_TEST_BIN_PATH}/zbc_test_dev_control -q -ur y ${device}
}

# Configure mutations to be tested

if [ -z "${ZBC_MUTATIONS}" -a  -z "${ZA_MUTATIONS}" -a -z "${WPC_MUTATIONS}" ]; then
    ZBC_MUTATIONS="HM_ZONED_1PCNT_B  HM_ZONED_2PCNT_BT  HA_ZONED_1PCNT_B"
		# HM_ZONED  HA_ZONED  HA_ZONED_2PCNT_BT  
    ZA_MUTATIONS="ZA_1CMR_BOT  ZA_1CMR_BOT_SWP  ZA_FAULTY"
		# ZA_1CMR_BOT_TOP  ZONE_ACT  ZA_1CMR_BT_SMR  ZA_BARE_BONE  ZA_STX
    WPC_MUTATIONS="ZA_WPC  ZA_WPC_EMPTY  ZA_WPC_SWP"
fi

#XXX SPEC needs resolving
# Set to 0 configures test scripts to expect INACTIVE, OFFLINE, RDONLY checks done after boundary checks
# Set to 1 configures test scripts to expect INACTIVE, OFFLINE, RDONLY checks done before boundary checks
if [ -z "${CHECK_ZC_BEFORE_ZT}" ]; then
	export CHECK_ZC_BEFORE_ZT=1		#XXX vary order of OP error checks
fi

# Pass this environment variable through to children if it is set
if [ ! -z "${ATA_SCSI_NONSENSE}" ]; then
    export ATA_SCSI_NONSENSE=1
fi

# Establish log file for early failures
if [ -z "${ZBC_TEST_LOG_PATH_BASE}" ]; then
    ZBC_TEST_LOG_PATH_BASE=log/${dev_name}
fi
export ZBC_TEST_LOG_PATH_BASE
set_logfile ""

# Run the tests
if [ -n "${ZBC_TEST_SECTION_LIST}" ] ; then
    # We are being invoked recursively with a specified Section list.
    prepare_lists ${ZBC_TEST_SECTION_LIST}
    zbc_run_config ${ZBC_TEST_SECTION_LIST}
else
    # Section 03 contains ZA tests that should be run once for each mutation.
    #
    # Section 04 recursively invokes this script multiple times per mutation,
    # each time with a different activation configuration (pure CMR and mixed
    # CMR/SMR).  The child script instances process ZBC_TEST_SECTION_LIST.
    prepare_lists "03" "04"			# parent section list

    if [ ${force_ata} -eq 0 ]; then
	# Sections 00, 01, and 02 contain ZBC (non-ZA) scripts.
	# Section 05 has ZA tests that should run once per activation config.
	# Section 08 has tests that are only valid on SCSI drives.
	# Section 09 has site-local tests.
	export ZBC_TEST_SECTION_LIST="00 01 02 05 08 09" # for SCSI ZA meta-children
    else
	# Omit Section 08_scsi_only for ATA drives
	export ZBC_TEST_SECTION_LIST="00 01 02 05 09"	 # for ATA ZA meta-children
    fi

    zbc_run_gamut
fi
