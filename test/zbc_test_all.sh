#!/bin/bash
# Run zbc_dhsmr_test.sh on multiple real and/or emulated devices in parallel.

usage_exit()
{
    echo "Usage: zbc_test_all.sh [ script_args ] [ ${main_script}_args ]"
    echo "      --emu   test emulator mutations"
    echo "      --real  test real devices using native command set"
    echo "      --sat   test real ATA devices using SCSI-ATA-Translation (SAT)"
    echo "Runs all tests if no script_args are specified"
    exit 1
}

LOG=log
main_script=./zbc_dhsmr_test.sh
ZBC_TEST_REAL_OK=zbc_test_real_ok	# file specifying real device names
ZBC_TEST_REAL_PEND=zbc_test_real_pending

##### Testing of emulated devices #####

# Run test suite on each mutation in the argument list, using all available emulator instances.
# Function returns after all the tests it starts have completed.
test_mutation_list()
{
    # Get the list of sg device names associated with emulator instances
    local emu_dev_list=`lsscsi -g | grep "TCMU DH-SMR" | sed -e "s/  *$//" -e "s/.* //" | sort -tg -nk2`
    if [ -z "${emu_dev_list}" ]; then
	echo "Found no emulator devices"
	return 1
    fi

    ndev=`echo ${emu_dev_list} | wc -w`
    nmut=$#
    echo `date` "====== Start testing of ${nmut} emulator mutations with ${ndev} available emulator instances ======"

    # Start one mutation testing on each available emulator instance
    local dev
    for dev in ${emu_dev_list}; do
	local dev_name=`basename ${dev}`
	local dir_name=${dev_name}_${1}

	# ${main_script} will run the mutation(s) specified in the environment variable(s) --
	# ZD_MUTATIONS run the Zone Domains tests; ZBC_MUTATIONS limit testing to the original ZBC standard.
	if [ ${1:0:1} = "Z" ]; then
	    echo `date`      "ZD_MUTATIONS=$1 ${main_script} ${cmdline_flags} -b ${dev} >& ${LOG}/${dir_name}.out"
	    ZD_MUTATIONS=$1  /usr/bin/time -p ${main_script} ${cmdline_flags} -b ${dev} >& ${LOG}/${dir_name}.out &
	else
	    echo `date`     "ZBC_MUTATIONS=$1 ${main_script} ${cmdline_flags} -b ${dev} >& ${LOG}/${dir_name}.out"
	    ZBC_MUTATIONS=$1 /usr/bin/time -p ${main_script} ${cmdline_flags} -b ${dev} >& ${LOG}/${dir_name}.out &
	fi

	shift
	if [ -z "$1" ]; then
	    # We have started testing the last mutation
	    # Wait for the last round of test instances to complete
	    wait
	    echo `date` "====== Completed testing of emulator mutations ======"
	    return 0
	fi
    done

    # XXX It would be more optimal to re-use emulator instances individually as each completes
    #     a mutation, instead of reusing the group after *all* of them have finished a round.

    # Wait for the current round of test instances to complete
    wait
    echo ""
    echo `date` "====== Starting another round of emulator mutation testing ======"

    # Start another round of test instances on the remaining mutations
    test_mutation_list "$@"
}

zbc_test_emu()
{
    # Test these mutations against original ZBC standard (names do not start with "Z")
    local HM_LIST="HM_ZONED_1PCNT_B   HM_ZONED_2PCNT_BT   HM_ZONED_FAULTY"
    local HA_LIST="HA_ZONED_1PCNT_B   HA_ZONED_2PCNT_BT"

    # Test these mutations against Zone Domains standard (names start with "Z")
    local ZD_BASE="ZONE_DOM           ZD_BARE_BONE        ZD_STX"
  # local ZD_SWR=" ZD_1CMR_BOT        ZD_1CMR_BOT_TOP     ZD_FAULTY    ZD_1CMR_BT_SMR"
    local ZD_SWR=" ZD_1CMR_BOT                            ZD_FAULTY"	#XXX
    local ZD_SWP=" ZD_1CMR_BOT_SWP    ZD_SOBR_SWP"
    local ZD_SOBR="ZD_SOBR            ZD_SOBR_EMPTY       ZD_SOBR_FAULTY"

    test_mutation_list ${ZD_SWR} ${ZD_SOBR} ${ZD_SWP} ${ZD_BASE} ${HM_LIST} ${HA_LIST}
}

##### Testing of real devices #####

# Run test suite on all real devices in the argument list in parallel.
# Returns after all the tests started here have completed.
# Arg list consists of zero or more triples:
#	device_path   flags_to_zbc_dhsmr_test.sh   logging_name
test_device_list()
{
    # Start a test suite instance running on each device in our arg list
    while [ ${1} ]; do
	local dev="$1"	    ; shift
	local flags="$1"    ; shift
	local name="$1"	    ; shift

	local dev_name=`basename ${dev}`
	local dir_name=${dev_name}_${name}

	echo `date`     "${main_script} ${flags} -b ${dev} >& ${LOG}/${dir_name}.out"
	/usr/bin/time -p ${main_script} ${flags} -b ${dev} >& ${LOG}/${dir_name}.out &
    done

    # Wait for all the test instances to finish
    wait
}

# Run the test suite on real devices specified by one or more of
# ${ZBC_SCSI}, ${ZBC_SATA}, and ${ZBC_SAS_SATA}
# Set ${test_sat} non-empty to test ATA drives in SAT mode (in addition to native ATA mode)
zbc_test_real()
{
    # zbc_dhsmr_test.sh flags for testing real devices:
    #   04.010 tests the device's configuration once, without any attempted mutation.
    #   Skip Sections 05 and 07 (ZA tests) for real drives which at present are all non-ZA.
    local real_flags="-e 04.010 -s 05.* -s 07.* $@"

    # zbc_dhsmr_test.sh flags for testing SATA drives:
    #   Skip Sections 07 and 08 (SCSI-only tests) for SATA drives
    #   SATA drives need these flags with or without --ata
    local sata_flags="-s 07.* -s 08.*"

    # We test SATA drives in two modes: native ATA commands and SCSI-ATA-Translation (SAT).
    # Run the test in two phases to avoid testing the same drive with SCSI and ATA
    # commands concurrently,
    local native_test_list=()
    local SAT_test_list=()

    # Construct the test lists based on what devices we have
    if [ ${ZBC_SCSI} ]; then
	native_test_list+=( ${ZBC_SCSI} \
			"${real_flags} ${cmdline_flags}" "scsi")
    fi
    if [ ${ZBC_SATA} ]; then
	native_test_list+=(  ${ZBC_SATA}
			"${real_flags} ${cmdline_flags} ${sata_flags} --ata" "ata")
	SAT_test_list+=( ${ZBC_SATA}
			"${real_flags} ${cmdline_flags} ${sata_flags}" "sat_kernel")
    fi
    if [ ${ZBC_SAS_SATA} ]; then
	native_test_list+=( ${ZBC_SAS_SATA}
			"${real_flags} ${cmdline_flags} ${sata_flags} --ata" "ata")
	SAT_test_list+=( ${ZBC_SAS_SATA}
			"${real_flags} ${cmdline_flags} ${sata_flags}" "sat_hba")
    fi

    if [ ${test_real} ]; then
	echo `date` "====== Start testing of real devices using native commands ======"
	test_device_list "${native_test_list[@]}"
    fi

    if [ ${test_sat} ]; then
	echo ""
	echo `date` "====== Start testing of real ATA devices using SAT ======"
	test_device_list "${SAT_test_list[@]}"
    fi

    echo `date` "====== Completed testing of real devices ======"
}

# Return the (sg) pathnames of real ZBC devices for testing.
# Device names are returned via ${ZBC_SCSI}, ${ZBC_SATA}, and ${ZBC_SAS_SATA},
# each either empty or containing one device name.
get_real_device_names()
{
    # If any of the ZBC_* are already set from the environment, use those
    if [ "${ZBC_SCSI}" -o "${ZBC_SATA}" -o "${ZBC_SAS_SATA}" ]; then
	echo "Devices: scsi=\"${ZBC_SCSI}\" sata=\"${ZBC_SATA}\" sas_sata=\"${ZBC_SAS_SATA}\""
	return 0
    fi

    # Set the device names from a file, if it exists
    if [ -f ${ZBC_TEST_REAL_OK} ]; then
	. ${ZBC_TEST_REAL_OK}
	echo "Devices: scsi=\"${ZBC_SCSI}\" sata=\"${ZBC_SATA}\" sas_sata=\"${ZBC_SAS_SATA}\""
	return 0
    fi

    # Lookup the available devices from `lsscsi -g` -- for example:
    # [4:0:0:0]    zbc     ATA      HGST HSH721414AL T204  /dev/sdc   /dev/sg2
    # [8:0:0:0]    zbc     ATA      HGST HSH721414AL T204  /dev/sdd   /dev/sg3
    # [8:0:2:0]    zbc     HGST     HSH721414AL52M0  a204  /dev/sdf   /dev/sg5
    # This is bogus, but it works for my drive configuration...

    declare -g \
          ZBC_SCSI=`lsscsi -g | grep " zbc " | grep -v " ATA " | head -1 | sed -e "s/  *$//" -e "s/.* //"`

    local DEV_ATA1=`lsscsi -g | grep " zbc " | grep    " ATA " | head -1 | sed -e "s/  *$//" -e "s/.* //"`
    local DEV_ATA2=`lsscsi -g | grep " zbc " | grep    " ATA " | tail -1 | sed -e "s/  *$//" -e "s/.* //"`

    local HBA_SCSI=`lsscsi -g | grep " zbc " | grep -v " ATA " | head -1 | sed -e "s/\[//" -e "s/] .*$//"`
    local HBA_ATA1=`lsscsi -g | grep " zbc " | grep    " ATA " | head -1 | sed -e "s/\[//" -e "s/] .*$//"`
    local HBA_ATA2=`lsscsi -g | grep " zbc " | grep    " ATA " | tail -1 | sed -e "s/\[//" -e "s/] .*$//"`

    # Assume a SATA device on the same HBA as a SCSI device is SAS/SATA
    if [ "${HBA_ATA1:0:1}" = "${HBA_SCSI:0:1}" ]; then
	ZBC_SAS_SATA=${DEV_ATA1}
    elif [ "${HBA_ATA2:0:1}" = "${HBA_SCSI:0:1}" ]; then
	ZBC_SAS_SATA=${DEV_ATA2}
    fi

    # Assume a SATA device NOT on the same HBA as our SCSI device is AHCI/SATA
    if [ "${HBA_ATA1:0:1}" != "${HBA_SCSI:0:1}" ]; then
	ZBC_SATA=${DEV_ATA1}
    elif [ "${HBA_ATA2:0:1}" != "${HBA_SCSI:0:1}" ]; then
	ZBC_SATA=${DEV_ATA2}
    fi

    # Let's be paranoid about automatically choosing real devices to do (write) testing on
    echo "ZBC_SCSI=${ZBC_SCSI}"		>  ${ZBC_TEST_REAL_PEND}
    echo "ZBC_SATA=${ZBC_SATA}"		>> ${ZBC_TEST_REAL_PEND}
    echo "ZBC_SAS_SATA=${ZBC_SAS_SATA}"	>> ${ZBC_TEST_REAL_PEND}
    chmod 666 ${ZBC_TEST_REAL_PEND}

    cat ${ZBC_TEST_REAL_PEND}
    echo "If those devices are OK for testing, then  \"mv ${ZBC_TEST_REAL_PEND} ${ZBC_TEST_REAL_OK}\"  and try again."
    echo "Otherwise edit ${ZBC_TEST_REAL_PEND} to suit your system and then rename the file as above."

    return 1
}

##### Main function #####

zbc_test()
{
    while [ "${1:0:2}" = "--" ]; do
	case "$1" in
	"--emu")    test_emu=1 ;;
	"--real")   test_real=1 ;;
	"--sat")    test_sat=1 ;;
	"*")	    break;;
	esac
	shift
    done

    # If none of our args were specified, run all the tests by default
    if [ -z "${test_emu}" -a -z "${test_real}" -a -z "${test_sat}" ]; then
	test_emu=1
	test_real=1
	test_sat=1
    fi

    local test_list=()
    if [ -n "${test_real}" -o -n "${test_sat}" ]; then
	# Get the names of the real devices to use in testing
	get_real_device_names
	if [ $? -ne 0 ]; then
	    exit 1
	fi
	test_list+=( "zbc_test_real" )
    fi
    if [ ${test_emu} ]; then
	test_list+=( "zbc_test_emu" )
    fi

    # Any remaining args left on the command line go to ${main_script}
    cmdline_flags="$@"

    # Start all the tests running
    for t in "${test_list[@]}" ; do
	${t} &
	sleep .25	# tidier startup messages
    done

    # Wait for all the tests to complete
    wait

    # Print a list of failure information from the summary logs
    echo ""
    grep -i fail ${LOG}/*.out

    # Print the log filenames containing test failures, sorted by script number
    echo ""
    echo "Log files showing failing tests:"
    grep -i =f `find ${LOG} -type f` \
     | grep -v "/04/0.0.log" \
     | sed -e 's/.* //' -e '\,/04/,!s,[0-9]/,&//,' \
     | sort -t/ -k7 \
     | sed -e 's/\/\/*/\//'

    # Try to detect script problems
    echo ""
    egrep -i "usage:|no such file|command not found|syntax error|unknown|warning:" `find ${LOG} -type f` \
     | egrep -v 'Unknown-additional|Unknown-sense-key' \
     | egrep -v 'because maxact_control=0'
}

if [ "$1" = "-h" -o "$1" = "--help" ]; then
    usage_exit
fi

mkdir -p ${LOG}

zbc_test "$@"
