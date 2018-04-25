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

function zbc_print_usage()
{

	echo "Usage: $0 [Options] [Device path]"
	echo "Options"
	echo "  -h | --help              : Print this usage"
	echo "  -l | --list              : List all test cases"
	echo "  -b | --batch             : Use batch mode (do not stop on failed tests)"
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
    exit
fi

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
)

for p in ${test_progs[@]}; do
	path=${ZBC_TEST_BIN_PATH}/${p}
	if [ ! -e ${path} ]; then
		echo "Test program ${p} not found in directory ${ZBC_TEST_BIN_PATH}"
		echo "Re-configure with option \"--with-test\" and recompile"
		exit
	fi
done

ZBC_TEST_SCR_PATH=scripts
if [ ! -d ${ZBC_TEST_SCR_PATH} ]; then
    echo "Test script directory ${ZBC_TEST_SCR_PATH} does not exist"
    exit
fi

# Handle arguments
argv=("$@")
argc=$#
argimax=$((argc-1))

exec_list=()
skip_list=()
print_list=0
export batch_mode=0
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
	-e | --exec )
		i=$((i+1))
		exec_list+=(${argv[$i]})
		;;
	-s | --skip )
		i=$((i+1))
		skip_list+=(${argv[$i]})
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

if [ ${force_ata} -eq 1 ]; then
	export ZBC_TEST_FORCE_ATA="ATA"
else
	unset ZBC_TEST_FORCE_ATA
fi

if [ ${print_list} -eq 1 ]; then
	exec_list=()
	skip_list=()
	device=""
fi

# Check device path
if [ ! -z ${device} ]; then
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

# Reset the DUT to factory conditions
function zbc_reset_test_device()
{
	vendor=`zbc_info ${device} | grep "Vendor ID: .*" | while IFS=: read a b; do echo $b; done`

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

# Build run list
function get_exec_list()
{
	for secnum in 03 04; do
		for file in ${ZBC_TEST_SCR_PATH}/${secnum}*/*.sh; do
			_IFS="${IFS}"
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
	testnum=$1
	_IFS="${IFS}"
	IFS='.'
	set -- ${testnum}
	echo "${1}"
	IFS="$_IFS"
}

function get_case_num()
{
	testnum=$1
	_IFS="${IFS}"
	IFS='.'
	set -- ${testnum}
	echo "${2}"
	IFS="$_IFS"
}

if [ ${#exec_list[@]} -eq 0 ]; then
	get_exec_list
fi

# Sort lists
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
		if [ "${e}" == "${s}" ]; then
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

# Run test cases of a section
function zbc_run_section()
{
	local ret=0
	local section_num="$1"
	local section_name="$2"

	section_path=`find ${ZBC_TEST_SCR_PATH} -type d -name "${section_num}*" -print`
	if [ -z "${section_path}" ]; then
		echo "Test script directory ${section_path} does not exist"
		exit
	fi

	log_path=${ZBC_TEST_LOG_PATH}/${section_num}
	mkdir -p ${log_path}

	if [ ${print_list} -eq 1 ]; then
		# Printing test cases only
		echo "Section ${section} - ${section_name} tests"
	else
		# Init: Close and reset all zones
		${ZBC_TEST_BIN_PATH}/zbc_test_close_zone ${device} -1
		${ZBC_TEST_BIN_PATH}/zbc_test_reset_zone ${device} -1
		echo "Executing section ${section} - ${section_name} tests..."
	fi

	# Execute test cases for this section
	for t in ${run_list[@]}; do

		s=`get_section_num ${t}`
		if [ "${s}" != "${section_num}" ]; then
			continue
		fi

		c=`get_case_num ${t}`
		./${section_path}/${c}.sh ${ZBC_TEST_BIN_PATH} ${log_path} ${section_num} ${device}
		ret=$?

		if [ ${batch_mode} -eq 1 ]; then
			continue
		fi

		if [ ${print_list} -ne 1 ]; then
			res="`cat ${log_path}/${c}.log | grep TESTRESULT`"
			if [ ${ret} -ne 0 -o "${res}" = "TESTRESULT==Failed" ]; then
				ret=1
				break
			fi
		fi

	done

	return ${ret}
}

# Reset the device if needed
if [ ${skip_format_dut} -eq 0 ]; then
    zbc_reset_test_device
    if [ $? -ne 0 ]; then
	echo "Can't reset test device"
	exit 1
    fi
    # Allow the main ACTIVATE tests to run unhindered
    zbc_dev_control -maxd unlimited ${device}
fi

# Run tests
function zbc_run_config()
{
    for section in ${section_list[@]}; do

	case "${section}" in
	"03")
		section_name="DH-SMR command check"
		;;
	"04")
		section_name="DH-SMR device ZBC"
		;;
	"05")
		section_name="DH-SMR zone checks"
		;;
	* )
		echo "Unknown test section ${section}"
		exit 1
		;;
	esac

	zbc_run_section "${section}" "${section_name}"
	if [ $? -ne 0 ]; then
		exit 1
	fi

    done
}

if [ -z ${ZBC_TEST_LOG_PATH_BASE} ] ; then
	ZBC_TEST_LOG_PATH_BASE=log/${dev_name}
fi

function zbc_run_gamut()
{
    echo "###### Run the entire dhsmr suite with URSWRZ enabled"
    ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH_BASE}/urswrz_y
    zbc_dev_control -ur y ${device}
    zbc_run_config "$@"

    echo "###### Run the entire dhsmr suite with URSWRZ disabled"
    ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH_BASE}/urswrz_n
    zbc_dev_control -ur n ${device}
    zbc_run_config "$@"

    # When done leave the device with URSWRZ set
    zbc_dev_control -ur y ${device}
}

export ZBC_TEST_LOG_PATH

zbc_run_gamut "$@"
