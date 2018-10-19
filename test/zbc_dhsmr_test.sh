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

export LC_ALL=C		# do sort(1) correctly

function zbc_print_usage()
{
	echo "Usage: $0 [Options] [Device path]"
	echo "Options"
	echo "  -h | --help              : Print this usage"
	echo "  -l | --list              : List all test cases"
	echo "  -v | --verbose           : Include extra detail on Passed summary lines"
	echo "  -b | --batch             : Use batch mode (do not stop on failed tests)"
	echo "       --valgrind          : Run test programs under valgrind"
	echo "  -a | --ata               : Force the use of the ATA backend driver for ZAC devices"
	echo "  -c | --current           : Only run the current mutation even if DUT supports MUTATE"
	echo "  -q | --quick             : Test only one activation per mutation and fewer mutations"
	echo "  -e | --exec <test number>: Execute only the specified test."
	echo "                             This option may be repeated multiple times"
	echo "                             to execute multiple tests in one run."
	echo "  -s | --skip <test number>: Skip execution of the specified test."
	echo "                             This option may be repeated multiple times"
	echo "                             to skip the execution of multiple tests."
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
    zbc_test_report_domains \
    zbc_test_report_realms \
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
_eexec_list=()		# -ee args passed down to ZD meta-children as -e
skip_list=()		# The skip_list argument may be a regular expression
_cskip_list=()		# meta-child inherits the skip_list
print_list=0
batch_mode=0		# If set, do not stop on first failed test script
force_ata=0
format_dut=0
skip_format_dut=0
current_mutation=0	# Only test the current mutation even if the DUT supports MUTATE

# Store argument
for (( i=0; i<${argc}; i++ )); do

	case ${argv[$i]} in
	-h | --help )
		zbc_print_usage
		;;
	-l | --list )
		print_list=1
		;;
	-b | --batch)
		batch_mode=1
		;;
	-v | --verbose )
		ZBC_TEST_PASS_DETAIL=1
		;;
	--valgrind )
		VALGRIND="valgrind --num-callers=24 --read-var-info=yes --leak-check=full --show-leak-kinds=all --track-origins=yes"
		;;
	-e | --exec )
		i=$((i+1))
		exec_list+=(${argv[$i]})
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
	--scsi )
		SCSI_ZD_SECTION="07"
		SCSI_ZBC_SECTION="08"
		;;
	-u | --unpublished )
		EXTRA_SECTION="09"
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
	-c | --current )
		current_mutation=1
		;;
	-q | --quick )
		ZBC_TEST_MUTATIONS="ZD_1CMR_BOT ZD_1CMR_BOT_SWP ZD_SOBR"
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

eexec_list="${_eexec_list[*]}"
cskip_list="${_cskip_list[*]}"

if [ ${print_list} -ne 0 ]; then
	exec_list=()
	skip_list=()
	device=""
fi

. scripts/zbc_test_lib.sh

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

	zbc_test_write_test_attribute
	wa_rc=$?
	if [[ ${wa_rc} -ne 0 ]]; then
	    echo  NO_WRITE_ATTRIBUTE=1 ${wa_rc}
	    export NO_WRITE_ATTRIBUTE=1		# inhibit sending this cmd
	fi

	zbc_test_write_dev_log
	wb_rc=$?
	if [[ ${wb_rc} -ne 0 ]]; then
	    echo  NO_WRITE_BUFFER=1 ${wb_rc}
	    export NO_WRITE_BUFFER=1		# inhibit sending this cmd
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
		get_exec_list $@
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
	local section_num="$1"
	local section_name="$2"

	local section_path=`find ${ZBC_TEST_SCR_PATH} -type d -name "${section_num}*" -print`
	if [ -z "${section_path}" ]; then
		echo "Test script directory ${section_path} does not exist"
		exit 1
	fi

	local log_path=${ZBC_TEST_LOG_PATH}/${section_num}

	if [ ${print_list} -ne 0 ]; then
		# Printing test cases only
		echo "Section ${section} - ${section_name} tests"
	else
		# Init: Reset all zones to EMPTY
		mkdir -p ${log_path}
		${ZBC_TEST_BIN_PATH}/zbc_test_reset_zone ${device} -1
		echo "Executing section ${section} - ${section_name} tests..."
	fi

	# Execute test cases for this section
	local -i sect_rc=0
	local t s c res
	for t in ${run_list[@]}; do

		s=`get_section_num ${t}`
		if [ "${s}" != "${section_num}" ]; then
			continue
		fi

		c=`get_case_num ${t}`

		# ZT_SOBR, ZT_CONV, and ZT_SWP are used by some scripts before zbc_test_lib.sh is sourced
		zbc_test_lib_init
		export ZT_SOBR
		export ZT_CONV
		export ZT_SWP
		export ZT_SWR
		export ZT_SEQ
		export ZT_NON_SEQ

		# Run the test case shell script
		./${section_path}/${c}.sh ${ZBC_TEST_BIN_PATH} ${log_path} ${section_num} ${device}
		sect_rc=$(( ${sect_rc} | $? ))

		if [ ${print_list} -ne 0 ]; then
		    continue
		fi

		res="`cat ${log_path}/${c}.log | grep TESTRESULT`"
		if [[ ${sect_rc} -ne 0 || ${res} =~ TESTRESULT==Failed* ]]; then
			sect_rc=1
			if [ ${batch_mode} -eq 0 ]; then
				break
			fi
		fi

	done

	return ${sect_rc}
}

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
    local sections="$@"
    local section_name
    local -i run_cfg_rc=0

    for section in ${sections} ; do

	case "${section}" in
	"00")
		section_name="ZBC command completion"
		;;
	"01")
		section_name="ZBC sense key, sense code"
		;;
	"02")
		section_name="ZBC zone state machine"
		;;
	"03")
		section_name="DH-SMR activation"
		;;
	"04")
		section_name="DH-SMR per-activation"
		;;
	"05")
		section_name="DH-SMR command and SOBR zone"
		;;
	"07")
		section_name="ZD SCSI-only"
		;;
	"08")
		section_name="ZBC SCSI-only"
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
	run_cfg_rc=$(( ${run_cfg_rc} | $? ))
	if [ ${run_cfg_rc} -ne 0 -a ${batch_mode} -eq 0 ]; then
		break
	fi

    done

    return ${run_cfg_rc}
}

function set_logfile()
{
    if [ -z "$1" ]; then
	ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH_DEV_BASE}
    else
	ZBC_TEST_LOG_PATH=${ZBC_TEST_LOG_PATH_DEV_BASE}/$1
    fi
    local log_path=${ZBC_TEST_LOG_PATH}
    mkdir -p ${log_path}
    log_file="${log_path}/zbc_dhsmr_test.log"
    rm -f ${log_file}
}

# Select the test Sections to be run, based on the device type (or mutation type).
# Run test suite on each URSWRZ setting the device supports.
# $1 is log subdirectory name
function zbc_run_mutation()
{
    zbc_test_get_device_info

    # Determine the list of Test Sections to run
    # Zone Domains devices:
    #   Section 03 contains per-mutation Zone Domain tests.
    #   Section 04 recursively invokes this script multiple times per mutation,
    #      each time with a different activation configuration (pure CMR and mixed
    #      CMR/SMR).  The child script instances process ZBC_TEST_SECTION_LIST.
    # ZBC (non-ZD) devices:
    #   One fixed configuration of zone types, invoked directly from this script
    device_is_ata=`lsscsi -g | egrep "${device} *$" | grep " ATA " | wc -l`	#XXX Reliable?

    # Sections 00, 01, and 02 contain ZBC (non-ZD) scripts.
    ZBC_TEST_SECTION_LIST="00 01 02"

    if [ ${zone_domains_device} -ne 0 ]; then
	# Include ZD tests in per-activation test runs
	ZBC_TEST_SECTION_LIST+=" 05"
	if [ ${device_is_ata} -eq 0 -a ${force_ata} -eq 0 ]; then
	    ZBC_TEST_SECTION_LIST+=" ${SCSI_ZD_SECTION}"
	    ZBC_TEST_SECTION_LIST+=" ${SCSI_ZBC_SECTION}"
	fi
	ZBC_TEST_SECTION_LIST+=" ${EXTRA_SECTION}"
	# Drive testing of Zone Domains devices through Sections 03 and 04
	prepare_lists "03 04"			# parent section list
    else
	# Not a Zone Domains device -- omit ZD tests
	if [ ${device_is_ata} -eq 0 -a ${force_ata} -eq 0 ]; then
	    ZBC_TEST_SECTION_LIST+=" ${SCSI_ZBC_SECTION}"
	fi
	ZBC_TEST_SECTION_LIST+=" ${EXTRA_SECTION}"
	# Drive testing of classic ZBC devices through Sections 00, 01, 02 (and 08 for SCSI)
	prepare_lists ${ZBC_TEST_SECTION_LIST}
	ZBC_TEST_SECTION_LIST=""	# Test sections run directly from this script
    fi

    if [ ${ur_control} -eq 0 ]; then
	# Run test suite on whichever URSWRZ setting the device supports
	if [ ${unrestricted_read} -eq 0 ]; then
	    echo "###### `date -Ins` Device supports URSWRZ disabled only"
	    set_logfile $1/urswrz_n
	else
	    echo "###### `date -Ins` Device supports URSWRZ enabled only"
	    set_logfile $1/urswrz_y
	fi

	reset_device
	zbc_run_config ${section_list[@]}
	return $?
    fi

    if [ ${skip_urswrz_n} ]; then
	local urswrz_list="y"
    else
	local urswrz_list="y n"
    fi

    # Run test suite on the selected URSWRZ setting(s)
    local -i mut_rc=0
    for urswrz in ${urswrz_list}; do
	echo -e "\n###### `date -Ins` Run the dhsmr test suite with URSWRZ=${urswrz}"
	set_logfile $1/urswrz_${urswrz}

	reset_device
	zbc_test_run ${ZBC_TEST_BIN_PATH}/zbc_test_dev_control -q -ur ${urswrz} ${device}

	mut_rc=$(( ${mut_rc} | $? ))
	if [ $? -ne 0 ]; then
	    echo "WARNING: Unexpected failure to set unrestricted read to "${urswrz}" on device ${device}"
	    if [ ${batch_mode} -eq 0 ]; then
		break
	    else
		continue
	    fi
	fi

	zbc_run_config ${section_list[@]}
	mut_rc=$(( ${mut_rc} | $? ))

	if [ ${mut_rc} -ne 0 -a ${batch_mode} -eq 0 ]; then
	    break
	fi
    done

    return ${mut_rc}
}

# Configure default set of mutations to be tested
function set_default_mutations()
{
    ZBC_TEST_MUTATIONS=""

#   ZBC_TEST_MUTATIONS+=" HM_ZONED"
#   ZBC_TEST_MUTATIONS+=" HM_ZONED_1PCNT_B"
    ZBC_TEST_MUTATIONS+=" HM_ZONED_2PCNT_BT"
    ZBC_TEST_MUTATIONS+=" HM_ZONED_FAULTY"

#   ZBC_TEST_MUTATIONS+=" HA_ZONED"
#   ZBC_TEST_MUTATIONS+=" HA_ZONED_1PCNT_B"
    ZBC_TEST_MUTATIONS+=" HA_ZONED_2PCNT_BT"

#   ZBC_TEST_MUTATIONS+=" ZONE_DOM"
#   ZBC_TEST_MUTATIONS+=" ZD_BARE_BONE"

#   ZBC_TEST_MUTATIONS+=" ZD_1CMR_BOT"
    ZBC_TEST_MUTATIONS+=" ZD_1CMR_BOT_TOP"
#   ZBC_TEST_MUTATIONS+=" ZD_1CMR_BT_SMR"
    ZBC_TEST_MUTATIONS+=" ZD_1CMR_BOT_SWP"
    ZBC_TEST_MUTATIONS+=" ZD_FAULTY"

    ZBC_TEST_MUTATIONS+=" ZD_SOBR"
#   ZBC_TEST_MUTATIONS+=" ZD_SOBR_EMPTY"
    ZBC_TEST_MUTATIONS+=" ZD_SOBR_SWP"
    ZBC_TEST_MUTATIONS+=" ZD_SOBR_FAULTY"
    ZBC_TEST_MUTATIONS+=" ZD_1SOBR_BOT_TOP"
}

function do_exports()
{
    # Used by the child instance of this script to select test Sections
    export ZBC_TEST_SECTION_LIST

    # Used by this script and/or zbc_test_lib.sh
    export VALGRIND
    export ZBC_TEST_LOG_PATH_BASE
    export ZBC_TEST_PASS_DETAIL

    # Used by Section 04 scripts when invoking this script recursively
    export eexec_list		# transmit -ee args as -e
    export cskip_list		# transmit -s args

    # Transmit --batch flag to meta-children via Section 04 scripts
    if [ ${batch_mode} -ne 0 ]; then
	export ZBC_TEST_BATCH_MODE=1
    else
	export ZBC_TEST_BATCH_MODE=0
    fi

    # Transmit --ata flag to meta-children via Section 04 scripts
    if [ ${force_ata} -ne 0 ]; then
	export ZBC_TEST_FORCE_ATA="ATA"
    else
	unset ZBC_TEST_FORCE_ATA
    fi
}

function zbc_run_gamut()
{
    uname -a		# Record machine and kernel info

    if [ ${current_mutation} -ne 0 ]; then
	zbc_run_mutation ""	# test the device as it is already configured
	return $?
    fi

    zbc_test_get_device_info
    if [ ${mutations} -eq 0 ]; then
	# Device is not capable of mutation
	if [ ${ZBC_TEST_MUTATIONS} ]; then
	    # mutations were specified -- issue warning
	    echo -e "\nWARNING: Device doesn't support mutation -- ignoring ZBC_TEST_MUTATIONS"
	fi
	zbc_run_mutation ""	# test the device as it is already configured
	return $?
    fi

    if [ -z "${ZBC_TEST_MUTATIONS}" ]; then
	set_default_mutations
    fi

    local -i gamut_rc=0
    for m in ${ZBC_TEST_MUTATIONS} ; do
	echo -e "\n######### `date -Ins` Run the Zoned Block Device test suite under mutation ${m}"
	set_logfile ${m}

	zbc_test_run zbc_dev_control -v -mu ${m} ${device}  # fix and use zbc_test_dev_control
	if [ $? -ne 0 ]; then
	    echo "Device ${device} does not support mutation type ${m} -- skipping"
	    continue
	fi

	# subshell makes exec_list local to the mutation (see first line of prepare_lists)
	(
		zbc_run_mutation "${m}"
	)
	gamut_rc=$(( ${gamut_rc} | $? ))
	if [ ${gamut_rc} -ne 0 -a ${batch_mode} -eq 0 ]; then
	    break
	fi
    done

    return ${gamut_rc}
}

ata_name=""
if [ ${force_ata} -ne 0 ]; then
    ata_name="_ata"
fi

# Establish log file for early failures
if [ -z "${ZBC_TEST_LOG_PATH_BASE}" ]; then
    ZBC_TEST_LOG_PATH_BASE=log
fi
ZBC_TEST_LOG_PATH_DEV_BASE=${ZBC_TEST_LOG_PATH_BASE}/${dev_name}${ata_name}
set_logfile ""

do_exports

if [ ${print_list} -ne 0 ]; then
    # List all the tests in all the test sections
    prepare_lists  "00 01 02 03 04 05 ${SCSI_ZD_SECTION} ${SCSI_ZBC_SECTION} ${EXTRA_SECTION}"
    zbc_run_config "00 01 02 03 04 05 ${SCSI_ZD_SECTION} ${SCSI_ZBC_SECTION} ${EXTRA_SECTION}"
elif [ -n "${ZBC_TEST_SECTION_LIST}" ] ; then
    # We are being invoked recursively with a specified Section list.
    prepare_lists ${ZBC_TEST_SECTION_LIST}
    zbc_run_config ${ZBC_TEST_SECTION_LIST}
else
    # Top-level invocation -- run the suite
    zbc_run_gamut
fi
ret=$?			# capture our exit code

if [ ${ret} -eq 0 -a ! -z "${device}" ]; then
    zbc_test_write_test_attribute ""
fi

exit ${ret}
