#!/usr/bin/env bash
#-------------------------------------------------------------------------------
# Copyright (c) 2023, Arm Limited or its affiliates. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Prerequists: Make sure RMM ACS, RMM and TFA builds are complete
# Usage:
#   ./run.sh --model <fvp_binary_full_path> --bl1 <full_path_to_tf-a-bl1.bin> \
#             --fip <full_path_to_tf-a-fip.bin> --acs_build_dir <full_path_to_rmm-acs/build>
#
# Note : For ACS secure test, make sure acs_secure.bin is part of fip image.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Set defaults
#------------------------------------------------------------------------------
ACS_NS_PRELOAD_ADDR_DFLT=0x88000000
arg_dryrun=
arg_model=
arg_bl1=
arg_fip=
arg_acs_build_dir=
arg_acs_ns_preload_addr=${ACS_NS_PRELOAD_ADDR_DFLT}
# Run the test with a timeout so they can't loop forever.
arg_test_timeout=1000
arg_debug=
arg_trace=
arg_no_telnet=
suite_timeout_multiplier=3
test_report_logfile=
regression_report_logfile=
uart0_logfile=
uart1_logfile=
uart3_logfile=
tfa_rmm_logfile=

fvp_cmd=" -C bp.refcounter.non_arch_start_at_default=1 \
-C bp.refcounter.use_real_time=0 \
-C bp.ve_sysregs.exit_on_shutdown=1 \
-C cache_state_modelled=1 \
-C cluster0.NUM_CORES=4 \
-C cluster0.PA_SIZE=48 \
-C cluster0.ecv_support_level=2 \
-C cluster0.gicv3.cpuintf-mmap-access-level=2 \
-C cluster0.gicv3.without-DS-support=1 \
-C cluster0.gicv4.mask-virtual-interrupt=1 \
-C cluster0.has_arm_v8-6=1 \
-C cluster0.has_branch_target_exception=1 \
-C cluster0.has_rme=1 \
-C cluster0.has_rndr=1 \
-C cluster0.has_amu=1 \
-C cluster0.has_v8_7_pmu_extension=2 \
-C cluster0.max_32bit_el=-1 \
-C cluster0.restriction_on_speculative_execution=2 \
-C cluster0.restriction_on_speculative_execution_aarch32=2 \
-C cluster1.NUM_CORES=4 -C cluster1.PA_SIZE=48 \
-C cluster1.ecv_support_level=2 \
-C cluster1.gicv3.cpuintf-mmap-access-level=2 \
-C cluster1.gicv3.without-DS-support=1 \
-C cluster1.gicv4.mask-virtual-interrupt=1 \
-C cluster1.has_arm_v8-6=1 \
-C cluster1.has_branch_target_exception=1 \
-C cluster1.has_rme=1 -C cluster1.has_rndr=1 \
-C cluster1.has_amu=1 -C cluster1.has_v8_7_pmu_extension=2 \
-C cluster1.max_32bit_el=-1 \
-C cluster1.restriction_on_speculative_execution=2 \
-C cluster1.restriction_on_speculative_execution_aarch32=2 \
-C pci.pci_smmuv3.mmu.SMMU_AIDR=2 \
-C pci.pci_smmuv3.mmu.SMMU_IDR0=0x0046123B \
-C pci.pci_smmuv3.mmu.SMMU_IDR1=0x00600002 \
-C pci.pci_smmuv3.mmu.SMMU_IDR3=0x1714 \
-C pci.pci_smmuv3.mmu.SMMU_IDR5=0xFFFF0475 \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR1=0xA0000002 \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR2=0 \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR3=0 \
-C bp.pl011_uart0.out_file=- \
-C bp.pl011_uart1.out_file=- \
-C pctl.startup=0.0.0.0 \
-C cluster0.ish_is_osh=1 \
-C cluster1.ish_is_osh=1 \
-C bp.pl011_uart0.uart_enable=1 \
-C bp.pl011_uart1.uart_enable=1 \
-C bp.pl011_uart2.uart_enable=1 "

#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------

while (( "$#" )); do
case "$1" in
    # Machine configuration
    --model)
        arg_model="$2"
        shift 2
        ;;

    # Images
    --bl1)
        arg_bl1="$2"
        shift 2
        ;;
    --fip)
        arg_fip="$2"
        shift 2
        ;;
    --acs_build_dir)
        arg_acs_build_dir="$2"
        shift 2
        ;;
    --acs_ns_preload_addr)
        arg_acs_ns_preload_addr="$2"
        shift 2
        ;;

    # Other options
    --arg_test_timeout)
        arg_test_timeout="$2"
        shift 2
        ;;
    --debug)
        arg_debug=yes
        shift 1
        ;;
    --trace)
        arg_trace=yes
        shift 1
        ;;
    --no_telnet)
        arg_no_telnet=yes
        shift 1
        ;;
    -n | --dry-run)
        arg_dryrun=yes
        shift 1
        ;;
    -h | -help | --help | -usage | --usage)
        print_help
        exit 0
        ;;
    --) #Pass remaining command line items to model
        shift
        break
        ;;

    *)
        echo "Warning: unrecognised argument: $1" >&2
        print_help
        exit 1
        ;;
esac
done

#------------------------------------------------------------------------------
# Post-processing
#------------------------------------------------------------------------------
if [[ ${arg_model} = "" ]] || [[ ! -f ${arg_model} ]]
then
    echo "Error! --model parameter not set properly"
    exit 1
elif [[ ${arg_bl1} = "" ]] || [[ ! -f ${arg_bl1} ]]
then
    echo "Error! --bl1 parameter not set properly"
    exit 1
elif [[ ${arg_fip} = "" ]] || [[ ! -f ${arg_fip} ]]
then
    echo "Error! --fip parameter not set properly"
    exit 1
elif [[ ${arg_acs_build_dir} = "" ]] || [[ ! -d ${arg_acs_build_dir} ]]
then
    echo "Error! --acs_build_dir parameter not set properly"
    exit 1
fi

fvp_cmd="${arg_model} ${fvp_cmd} \
-C bp.flashloader0.fname=${arg_fip} \
-C bp.secureflashloader.fname=${arg_bl1} "

# Add additional model parameters from the cmdline
fvp_cmd="${fvp_cmd} $@"

regression_report_logfile=${arg_acs_build_dir}/../../../out/uart2.log
uart0_logfile=${arg_acs_build_dir}/../../../out/uart0.log
uart1_logfile=${arg_acs_build_dir}/../../../out/uart1.log
uart3_logfile=${arg_acs_build_dir}/../../../out/uart3.log

if [[ -f "${arg_acs_build_dir}/output/acs_non_secure.bin" ]]
then
    tfa_rmm_logfile=${arg_acs_build_dir}/output/tfa_rmm.log

    fvp_cmd="${fvp_cmd} \
 --data cluster0.cpu0=${arg_acs_build_dir}/output/acs_non_secure.bin@${arg_acs_ns_preload_addr}\
 -C bp.pl011_uart0.out_file=${uart0_logfile}\
 -C bp.pl011_uart1.out_file=${uart1_logfile}\
 -C bp.pl011_uart3.out_file=${uart3_logfile}\
 -C bp.pl011_uart2.out_file=${regression_report_logfile}\
 -C bp.pl011_uart2.unbuffered_output=1"

    if [[ ${arg_debug} == "yes" ]]
    then
    fvp_cmd="${fvp_cmd} --cadi-server"
    fi

    if [[ ${arg_trace} == "yes" ]]
    then
    fvp_cmd="${fvp_cmd} \
 --plugin ${arg_acs_build_dir}/../../../third-party/fvp/Base_RevC_AEMvA_pkg/plugins/Linux64_GCC-9.3/TarmacTrace.so \
 -C TRACE.TarmacTrace.trace_events=1 \
 -C TRACE.TarmacTrace.trace_instructions=1 \
 -C TRACE.TarmacTrace.start-instruction-count=20000000 \
 -C TRACE.TarmacTrace.trace_core_registers=1 \
 -C TRACE.TarmacTrace.trace_vfp=1 \
 -C TRACE.TarmacTrace.trace_mmu=0 \
 -C TRACE.TarmacTrace.trace_loads_stores=1 \
 -C TRACE.TarmacTrace.trace_cache=0 \
 -C TRACE.TarmacTrace.updated-registers=1 \
 -C TRACE.TarmacTrace.trace-file=${arg_acs_build_dir}/../../../out/trace.log"
    fi

    if [[ ${arg_no_telnet} == "yes" ]]
    then
    fvp_cmd="${fvp_cmd} \
 -C bp.terminal_0.start_telnet=0 \
 -C bp.terminal_1.start_telnet=0 \
 -C bp.terminal_2.start_telnet=0 \
 -C bp.terminal_3.start_telnet=0"
    fi

    arg_test_timeout=$(($arg_test_timeout * $suite_timeout_multiplier))

    echo "Running model command: timeout $arg_test_timeout $fvp_cmd | tee ${tfa_rmm_logfile}"
    if [[ ${arg_dryrun} != "yes" ]]
    then
        # delete any older logfile
        rm -f $regression_report_logfile $tfa_rmm_logfile
        # Execute the command
        timeout $arg_test_timeout $fvp_cmd | tee ${tfa_rmm_logfile}
        #Generate regression summary
    fi
    echo "Model command completed"
else
    cd ${arg_acs_build_dir}/output/
    for suite in */;do
        cd $suite

        for testcase in */; do
                if [[ -f "${arg_acs_build_dir}/output/$suite/$testcase/acs_non_secure.bin" ]]
                then
                    test_report_logfile=${arg_acs_build_dir}/output/$suite/$testcase/test_report.log
                    tfa_rmm_logfile=${arg_acs_build_dir}/output/$suite/$testcase/tfa_rmm.log
                    fvp_cmd_test="${fvp_cmd} \
 --data cluster0.cpu0=\
${arg_acs_build_dir}/output/$suite/$testcase/acs_non_secure.bin@${arg_acs_ns_preload_addr} \
 -C bp.pl011_uart2.out_file=${test_report_logfile}"

                    msg="Running model command for $testcase: timeout
                         $arg_test_timeout $fvp_cmd_test | tee -a ${tfa_rmm_logfile}"
                    echo $msg | tee -a ${tfa_rmm_logfile}
                    if [[ ${arg_dryrun} != "yes" ]]
                    then
                    # Execute the command
                    rm -f $test_report_logfile $tfa_rmm_logfile
                    timeout $arg_test_timeout $fvp_cmd_test | tee -a ${tfa_rmm_logfile}
                    #Generate regression summary
                    fi
                    echo "Model command completed"
                fi
        done

        cd -
    done
    # Gether logs from all tests into one file
    find . -type f | grep "test_report.log\b" | xargs cat | tee $regression_report_logfile
fi

total_tests=`grep -c ": Test=" $regression_report_logfile`
total_pass=`grep -c "Result => Passed" $regression_report_logfile`
total_skip=`grep -c "Result => Skipped" $regression_report_logfile`
#total_fail=`grep -c "Result => Failed" $regression_report_logfile`
total_fail=$(($total_tests - $total_pass - $total_skip))

echo "
***************************
Regression Summary:
Total Tests :$total_tests
Total Pass  :$total_pass
Total Skip  :$total_skip
Total Fail  :$total_fail
***************************"
exit 0

#------------------------------------------------------------------------------
# Functions
#------------------------------------------------------------------------------
function print_help()
{
    echo "run.sh [options] [-- [model_options]]"
    echo ""
    echo "Model Configuration:"
    echo "  --model PATH           <path_to_model_bin>"
    echo ""
    echo "Images:"
    echo "  --bl1                  <path_to_bl1.bin>"
    echo "  --fip                  <path_to_fip.bin>"
    echo "  --acs_build_dir        <path_to_acs_build_directory>"
    echo "  --acs_ns_preload_addr  <Address where acs_non_secure.bin to be preloaded>"
    echo "                       (default: ${ACS_NS_PRELOAD_ADDR_DFLT})"
    echo "Other options:"
    echo "  --test_timeout          Run each test with specified timeout in seconds"
    echo "                          (default: ${arg_test_timeout}s)"
    echo "  --help                  Print this message"
    echo "  -n / --dry-run          Print command but don't execute anything"
    echo ""
    echo "Options passed after an empty '--' are passed straight to the model"
}

