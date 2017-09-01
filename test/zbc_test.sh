#!/bin/bash
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
	echo "Only root can do this."
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

ZBC_TEST_LOG_PATH=log

# Handle arguments
argv=("$@")
argc=$#
argimax=$((argc-1))

exec_list=()
skip_list=()
print_list=0
batch_mode=0
force_ata=0

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
fi

# Build run list
function get_exec_list()
{
	for secnum in 00 01 02; do
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

function get_sect_num()
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

# Substract skip list from exec list. At the same time,
# extract the section list.
run_list=()
sect_list=()
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
	sect_num=`get_sect_num ${e}`
	sect_list+=(${sect_num})

done

sect_list=(`for s in ${sect_list[@]}; do echo "${s}"; done | sort -u`)

# Run test cases of a section
function zbc_run_section()
{
	local ret=0
	local sect_num="$1"
	local sect_name="$2"

	sect_path=`find ${ZBC_TEST_SCR_PATH} -type d -name "${sect_num}*" -print`
	if [ -z "${sect_path}" ]; then
		echo "Test script directory ${sect_path} does not exist"
		exit
	fi

	log_path=${ZBC_TEST_LOG_PATH}/${dev_name}/${sect_num}
	mkdir -p ${log_path}

	if [ ${print_list} -eq 1 ]; then
		# Printing test cases only
		echo "Section ${sect} - ${sect_name} tests"
	else
		# Init: Close and reset all zones
		${ZBC_TEST_BIN_PATH}/zbc_test_close_zone ${device} -1
		${ZBC_TEST_BIN_PATH}/zbc_test_reset_zone ${device} -1
		echo "Executing section ${sect} - ${sect_name} tests..."
	fi

    	# Execute test cases for this section
    	for t in ${run_list[@]}; do

		s=`get_sect_num ${t}`
		if [ "${s}" != "${sect_num}" ]; then
			continue
		fi

		c=`get_case_num ${t}`
        	./${sect_path}/${c}.sh ${ZBC_TEST_BIN_PATH} ${log_path} ${sect_num} ${device}
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

# Run tests
for sect in ${sect_list[@]}; do

	case "${sect}" in
	"00")
		sect_name="command completion"
		;;
	"01")
		sect_name="sense key, sense code"
		;;
	"02")
		sect_name="zone state machine"
		;;
	* )
		echo "Unknown test section ${sect}"
		exit 1
		;;
	esac

	zbc_run_section "${sect}" "${sect_name}"
	if [ $? -ne 0 ]; then
		exit 1
	fi

done
