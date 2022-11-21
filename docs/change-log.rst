Change Log & Release Notes
==========================

Please note that the Trusted Firmware-A Tests version follows the Trusted
Firmware-A version for simplicity. At any point in time, TF-A Tests version
`x.y` aims at testing TF-A version `x.y`. Different versions of TF-A and TF-A
Tests are not guaranteed to be compatible. This also means that a version
upgrade on the TF-A-Tests side might not necessarily introduce any new feature.

Version 2.8
-----------

New features
^^^^^^^^^^^^
-  More tests are made available in this release to help validate the
   functionalities in the following areas:

   - FF-A Features
   - Realm Management Extension
   - New Architecture Specific features related to v8.8
   - New platform ports

TFTF
~~~~

-  FF-A testing:

    - UUID included in partition information descriptors.
    - Checks for size of partition information descriptors.
    - Renamed FFA_MSG_RUN ABI function to FFA_RUN and allowed it to return from
      Waiting state.
    - Made ffa_tests available for Ivy.
    - Updated verbose message log structure.
    - Prevented generate_json.sh from being called more than once by requiring
      a list of partitions to be supplied.
    - Added a temporary workaround for unexpected affinity info state to prevent
      a system panic.
    - Added test to exercise FFA_CONSOLE_LOG ABI.

    - FF-A v1.1 Secure interrupts

        - Added managed exit to first and second SP in call chain.
        - Added test to exercise managed exit by two SPs in a call chain.
        - Added tests to exercise NS interrupt being queued and signaled to SP.

-  New tests:

    - Tests for SVE operations in Normal World and discover SVE vector length.
    - Added cleanup TRNG service tests.
    - Added test for SMCCC_ARCH_WORKAROUND_3.
    - Updated PAuth helpers to support QARMA3 algorithm.
    - Added tests for RNG_TRAP.

-  Platforms:

    - SGI:

        - Introduced platform variant build option.
        - Re-organized header files.
        - Migrated to secure uart port for routing tftf logs.

    - N1SDP:

        - Added TFTF support for N1SDP.

    - RD-N2:

        - Added TFTF support for RD-N2.

    - RD-N2-Cfg1:

        - Added TFTF support for RD-N2-Cfg1.

    - RD-V1:

        - Added TFTF support for RD-V1.

-  Miscellaneous:

    - Added a missing ISB instruction in SME test.
    - Refactor to make some helper functions re-usable.
    - Updated build command to clean EL3 payload image.
    - Move renaming of the primary dts file for ivy partitions.
    - Added check that verifies if a platform supports el3_payload before
      building it.
    - Updated memory share test to meet Hafnium specification.
    - Updated toolchain requirements documentation.


Realm Management Extension (RME)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Added Realm payload management capabilities to TFTF to act as a NS Host.
    - Added test to verify that RMM and SPM can co-exist and work properly.
    - Added function to reset delegated buffers to non-delegated state.
    - Re-used existing wait_for_non_lead_cpus() function helper.
    - Refactored RMI FID macros to simplify usage.
    - Added userguide for realm payload testing.

Cactus (Secure-EL1 test partition)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Corrected some tests message types from ERROR to VERBOSE.
    - Increased the cactus number of xlat to allow the use of 48b PA size for
      memory sharing between SPs.
    - Introduced a new direct request message command to resume after managed
      exit.
    - Skip enabling virtual maintenance interrupts explicitly.
    - Allowed sender to resume interrupted target vCPU.
    - Added support for handling managed exit through vIRQ.
    - Added support for discovering interrupt IDs of managed exit signals.
    - Specified action in response to NS interrupt in manifest.

Ivy (Secure-EL0 test partition)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Allowed testing using VHE.
    - Allowed Ivy partitions to use ffa_helpers functions.
    - Requirement of common name for Ivy partitions for consistency.
    - Specified action in response to NS interrupt in manifest.

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    - Fixed SME header guard name.
    - Fixed response for incorrect direct message request for FF-A.

Version 2.7
-----------

New features
^^^^^^^^^^^^
-  More tests are made available in this release to help validate the
   functionalities in the following areas:

   - FF-A Features
   - New Architecture Specific features related to v8.7
   - New platform port

TFTF
~~~~

-  FF-A testing:

    - FF-A partition information structure is updated to include UUIDs.
    - Memory Management helper functions are refactored to fetch the details
      of smc call failures in tftf and cactus.
    - Added test to validate memory sharing operations from SP to NS-endpoint
      are denied by SPMC.
    - Added test to ensure an endpoint that sets its version to v1.0 receives
      v1.0 partition information descriptors as defined in v1.0 FF-A
      specification.
    - Added test to validate that memory is cleared on memory sharing operations
      between normal world and secure world.

    - FF-A v1.1 Secure interrupts

        - Added support to enhance the secure interrupt handling test.
        - Support for registering and unregistering custom handler that is
          invoked by SP at the tail end of the virtual interrupt processing.
        - Added support for querying the ID of the last serviced virtual interrupt.

-  New tests:

    - Added test to validate that realm region access is being prevented from
      normal world.
    - Added test to validate that secure region access is being prevented from
      normal world.
    - Added test to validate that secure region access is being prevented from
      realm world.
    - Added test to validate that root region access is being prevented from
      realm world.
    - Added a test for v8.7 Advanced floating-point behavior (FEAT_AFP).
    - Added a SPE test that reads static profiling system registers
      of available SPE version i.e. FEAT_SPE/FEAT_SPEv1p1/FEAT_SPEv1p2.
    - Added a test to validate functionality of WFET and WFIT instructions
      introduced by v8.7 FEAT_WFxT.
    - Added basic SME tests to ensure feature enablement by EL3 is proper for
      its usage at lower non-secure ELs.
    - Added test to check Data Independent timing (DIT) field of PSTATE is
      retained on exception.
    - Added test to ensure that EL3 has properly enabled access to FEAT_BRBE
      from non-secure ELs.

-  Platforms:

    - Add initial platform support for corstone1000.

    - TC:

        - Support for notification in tertiary SP manifest.

    - FVP:

        - Support to provide test memory addresses to validate the invalid
          memory access test from tftf(ns-el2).

-  Miscellaneous:

    - Added support to configure the physical/virtual address space for FVP.
    - Added common header file for defining macros with size to support all the
      platforms.
    - Introduced handler for synchronous exceptions (AArch64).
    - Added macros to extract the ISS portion of an ELx ESR exception syndrome
      register.
    - Support to dynamically map/unmap test region to validate invalid memory
      access tests.
    - Added support to receive boot information through secure partitions,
      according to the FF-A v1.1 EAC0 specification.
    - Added an helper API function from SPM test suite to initialize FFA-mailbox
      and enable FF-A based message with SP.
    - Updated the build string to display the rc-tagged version.

Cactus (Secure-EL1 test partition)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Added test for nonsecure memory sharing between Secure Partitions(SPs).
    - Added test to validate that a realm region cannot be accessed from secure
      world.
    - Added test to permit checking a root region cannot be accessed from secure
      world.
    - Extended the test command CACTUS_MEM_SEND_CMD to add support for memory
      sharing flags.
    - Added support to save the state of general purpose registers x0-x4 at the
      entry to cold boot and restore them before jumping to entrypoint of cactus.

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    - Fixed a bug to align RMI FIDs with SMCCC.
    - Fixed encoding of vCPU and receiver IDs in the FFA_NOTIFICATION_GET
      interface to comply with the FF-A v1.1 beta0 specification.
    - Fixed memory retrieve request attributes by enforcing them to be inner
      shareable rather than outer.
    - Fixed static memory mapping of EL3 in EL2.
    - Fixed a spurious error log message with memory share test.
    - Aligning RMI FIDs with SMCCC.
    - Fixed PSCI system suspend test suite execution in a four world system.
    - Configured the build system to use DWARF 4 standard for debug builds with
      ArmDS.
    - Introduced macro IRQ_TWDOG_INTID for the Tegra210, Tegra186 and Tegra194
      platforms to fix the compilation failures.

Version 2.6
-----------

New features
^^^^^^^^^^^^
-  More tests are made available in this release to help validate the
   functionalities in the following areas:

    - Firmware Framework for Arm A-profile(FF-A)
    - Realm Management Extensions(RME)
    - Embedded Trace Extension and Trace Buffer Extension (ETE and TRBE)

TFTF
~~~~

-  FF-A testing:

    - Update FF-A version to v1.1
    - Added helpers for SPM tests to check partition info of SPs from normal
      world.
    - Added tests to check for ffa_features supported.
    - Added test for FFA_RXTX_UNMAP ABI.
    - Added test for FFA_SPM_ID_GET.
    - FF-A v1.1 Notifications

        - Added test for notifications bitmap create and destroy ABIs.
        - Added test for notifications set and get ABIs.
        - Added test for notification INFO_GET ABI.
        - Added test to check notifications pending interrupt is injected into
          and handled by the expected vCPU in a MP setup.
        - Added test for signaling from MP SP to UP SP.
        - Added test to check notifications interrupt IDs retrieved with
          FFA_FEATURES ABI.
        - Added test to check functionality of notifications scheduled receiver
          interrupt.

    - FF-A v1.1 Secure interrupts

        - Added support for handling secure interrupts in Cactus SP.
        - Added several tests to exercise secure interrupt handling while SP
          is in WAITING/RUNNING/BLOCKED state.

-  New tests:

    - Enabled SVE tests
    - Added test for trace system registers access.
    - Added test for trace filter control registers access.
    - Added test for trace buffer control registers access.
    - Added test to check PSTATE in SDEI handler.
    - Added test to check if HCRX_EL2 is accessible.

-  Platforms:

    - TC0:

        - Support for direct messaging with managed exit.
        - Support for building S-EL0 Ivy partition.

    - FVP:

         - Update Cactus secure partitions to indicate Managed exit support.

-  Miscellaneous

    - Added random seed generation capability and ability to specify build
      parameters for SMC Fuzzer tool.

Cactus (Secure-EL1 test partition)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Added helper for Cactus SP sleep.
    - Added test commands to request use of notifications interfaces.
    - Added several commands that generate direct message requests to assist in
      testing secure interrupt handling and notifications features in FF-A v1.1
    - Added support for SP805 Trusted Watchdog module.

Ivy (Secure-EL1 test partition)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Add shim layer to Ivy partition and enable PIE.
    - Define Ivy partition manifest and use FF-A for message handling.
    - Prepare S-EL1/0 enviroment for enabling S-EL0 application.

Realm Management Extension(RME)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - Added tests to run RMI and SPM on multiple CPUs concurrently.
    - Added tests for multi CPU delegation and fail conditions.
    - Added tests to query RMI version on multiple CPUs.

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    - Fixed Ivy partition start address for TC0.
    - Fixed SP manifests to use little endian format UUID.
    - Fixed a bug in memory sharing test for Cactus SP.
    - Invalidate data cache for NS_BL1U and NS_BL2U images.
    - Fixed attributes to Read-Write only for memory regions described in partition
      manifests.

Version 2.5
-----------

New features
^^^^^^^^^^^^
-  More tests are made available in this release to help validate the
   functionalities in the following areas:
    -  True Random Number Generator (TRNG) test scenarios.
    -  Multicore / Power State Controller Interface (PSCI) tests.
    -  v8.6 Activity Monitors Unit (AMU) enhancements test scenarios.
    -  Secure Partition Manager (SPM) / Firmware Framework (FF-A) v1.0 testing.
        -  Interrupt Handling between Non-secure and Secure world.
        -  Direct messages and memory sharing between Secure Partitions(SP).
        -  Many tests to exercise FF-A v1.0 ABIs.
        -  SPM saving/restoring the NS SIMD context enabling a normal world FF-A
           endpoint (TFTF) and a secure partition to use SIMD vectors and
           instructions independently.

TFTF
~~~~

-  SPM / FF-A v1.0 testing.
    -  Refactor FF-A memory sharing tests
        -  Created helper functions to initialize ffa_memory_region and to send
           the respective memory region to the SP, making it possible to reuse
           the logic in SP-to-SP memory share tests.
        -  Added comments to document relevant aspects about memory sharing.

    -  Trigger direct messaging between SPs.
        -  Use cactus command 'CACTUS_REQ_ECHO_SEND_CMD' to make cactus SPs
           communicate with each other using direct message interfaces.

    -  Added helpers for SPM tests.
        -  Checking SPMC has expected FFA_VERSION.
        -  Checking that expected FF-A endpoints are deployed in the system.
        -  Getting global TFTF mailbox.

-  Replace '.inst' AArch64 machine directives with CPU Memory Tagging Extension
   instructions in 'test_mte_instructions' function.

-  Add build option for Arm Feature Modifiers.
    -  This patch adds a new ARM_ARCH_FEATURE build option to add support
       for compiler's feature modifiers.

-  Enable 8 cores support for Theodul DSU(DynamIQ Shared Unit) for the
   Total Compute (TC0) platform.

-  New tests:

    -  Remove redundant code and add better tests for TRNG SMCs.
         -  Tests that the Version, Features, and RND calls conform to the spec.

    -  New tests for v8.6 AMU enhancements (FEAT_AMUv1p1)
         -  Make sure AMU offsets are being saved and restored properly.

    -  Tests to request SP-to-SP memory share.

    -  SP-to-SP direct messaging deadlock test.
         -  TFTF sends CACTUS_REQ_DEADLOCK_CMD to cactus SP.

Cactus(Secure-EL1 test partition)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  Enable managed exit for primary cactus secure partition.

-  Helper commands needed for interrupt testing.

-  Add handler from managed exit FIQ interrupt.

-  Make ffa_id global.

-  Implement HF_INTERRUPT_ENABLE Hafnium hypervisor call wrapper. With this
   service, a secure partition calls into the SPMC to enable/disable a
   particular virtual interrupt.

-  Invalidate the data cache for the cactus image.

-  Helper commands needed for interrupt testing.
     -  CACTUS_SLEEP_CMD & CACTUS_INTERRUPT_CMD added.

-  Decouple exception handling from tftf framework.
    -  With new interrupt related tests coming up in Cactus, added separate
       exception handler code for irq/fiq in Cactus.

-  Hypervisor calls moved to a separate module.

-  Add secondary entry point register function.

-  Declare third SP instance as UP SP.

-  Provision a cold boot path for secondary cores (or secondary pinned
   execution contexts).

-  Tidy message loop, commands definitions, direct messaging API definitions.

-  Helpers for error logging after FF-A calls.

-  Properly placing Cactus test files.

-  Tidying FF-A Memory Sharing tests.

-  Use CACTUS_ECHO_CMD in direct message tests.

-  Refactor handling of commands.
    -  Added helper macros to define a command handler, build a command table
       in which each element is a pair of the handler and respective command
       ID. Available tests have been moved to their own command handler.

-  Extend arguments in commands responses.
    -  In the test commands framework, added template to extend number of
       values to include in a command response.

-  Check FF-A return is a valid direct response.
    -  Added a helper function to check if return of FFA_MSG_SEND_DIRECT_REQ
       is FFA_MSG_SEND_DIRECT_RESP.

-  FFA_MSG_DIRECT_RESP call extended to use 5 registers.

-  Added accessors for arguments from FF-A calls.
    -  Some accessors for arguments from FF-A calls, namely for func id, error
       code, and direct message destination/source.

-  Use virtual counter for sp_sleep.
    -  Changes sp_sleep() to use virtual counter instead of physical counter.

-  Checks if SIMD vectors are preserved in the normal world while transitioning
   from normal world to secure world and back to normal world.

-  Tidying common code to tftf and cactus.

-  Refactor cactus_test_cmds.h to incorporate static inline functions instead
   of macros to enforce type checking.

-  Removed reference to Hafnium in name from helper function and macro to
   make them generic.

-  For consistency added the cmd id 'CACTUS_MEM_SEND_CMD'.

-  Add command to request memory sharing between SPs.

-  Add & handle commands 'CACTUS_REQ_ECHO_CMD' and 'CACTUS_ECHO_CMD'.

-  Update README with list of sample partitions.

-  Remove reference to PSA from xml test file.

-  Reduce tests verbosity in release mode.
    -  Update few NOTICE messages to VERBOSE/INFO.

-  Fix conversion issues on cactus responses.

-  Create RXTX map/configure helper macros and use them.

-  Update OP-TEE version used for testing to 3.10.
    -  SPMC as S-EL1 tests using OP-TEE depend on a static binary stored as
       a CI file. This binary corresponds to a build of OP-TEE v3.10.

-  Add uart2 to device-regions node.
    -  First SP no longer has an open access to the full system peripheral
       range and devices must be explicitly declared in the SP manifest.

-  New tests:

    -  Test for exercising SMMUv3 driver to perform stage2 translation.

    -  Test handling of non-secure interrupt while running SP.

    -  Add secondary cores direct messaging test for SPM.

    -  Testing deadlock by FF-A direct message.
         -  Added command CACTUS_DEADLOCK_CMD to file cactus_test_cmds.h to create
            a deadlock scenario using FF-A direct message interfaces.

    -  Test SP-to-SP memory share operations
         -  Handle 'CACTUS_REQ_MEM_SEND_CMD' by sending memory to the receiver SP.

    -  Implemented test to validate FFA_RXTX_MAP ABI.

Version 2.4
-----------

New features
^^^^^^^^^^^^
-  More tests are made available in this release to help validate the
   functionalities in the following areas:
   -  SMCCC.
   -  New architecture specific features.
   -  FF-A features.
   -  New platform ports.

-  Various improvements to test framework and test suite such as documentation,
   removing un-necessary dependencies, etc.

TFTF
~~~~

-  Remove dependencies from FVP to generic code by converting some FVP platform
   specific macros to the common macros.

-  Remove make as a package dependency to compile TF-A test code.

-  Move defaults values and macro defs in a separate folder from Makefile.

-  Allow alternate stdout to be used apart from pl011 UART.

-  Get FVP platform's topology from build options to make FVP platform
   configuration more flexible and eliminate test errors when the platform
   is configured with number of CPUs less than default values in the makefile.

-  Update the FIP corrupt address which is used to corrupt BL2 image that helps
   to trigger firmware update process.

-  Add explicit barrier before sev() in tftf_send_event_common API to avoid
   core hang.

-  Align output properly on issuing make help_tests by removing dashes
   and sort tests.

-  Moved a few FVP and Juno specific defined from common header files to platform
   specific header files.

-  Replace SPCI with PSA FF-A in code as SPCI is now called as FF-A.

-  Add owner field to sp_layout generation to differentiate owner of SP which
   could either be Silicon Provider or Platform provider.

-  Add v8.5 Branch Target Identifier(BTI) support in TFTF.

-  Remove dependency on SYS_CNT_BASE1 to read the memory mapped timers.

-  Enables SError aborts for all CPUs, during their power on sequence.

-  Documentation:

   -  Use conditional assignment on sphinx variables so that they can be
      overwritten by environment and/or command line.

   -  Add support for documentation build as a target in Makefile.

   -  Update list of maintainers.

   -  Update documentation to explain how to locally build the documentation.

   -  Add .editorconfig from TF-A to define the coding style.

   -  Fix documentation to include 'path/to' prefix when specifying tftf.bin on
      make fip cmd.

   -  Use docker to build documentation.

   -  Replace SPCI with PSA FF-A in documentation as SPCI is now called
      as FF-A.

-  NVIDIA Tegra194:

   -  Skip CPU suspend tests requiring SGI as wake source as Tegra194 platforms
      do not support CPU suspend power down and cannot be woken up with an SGI.

   -  Disable some system suspend test cases.

   -  Create dummy SMMU context for system resume to allow the System Resume
      Firmware to complete without any errors or warnings.

   -  Increase RTC step value to 5ms as RTC consumes 250us for each register
      read/write. Increase the step value to 5ms to cover all the register
      read/write in program_timer().

   -  Skip some timer framework validation tests as CPUs on Tegra194 platforms
      cannot be woken up with the RTC timer interrupt after power off.

   -  Introduce per-CPU Hypervisor Timer Interrupt ID.

   -  Skip PSCI STAT tests requiring PSTATE_TYPE_POWERDOWN as Tegra194 platforms
      do not support CPU suspend with state type as PSTATE_TYPE_POWERDOWN.

   -  Disable boot requirement tests as Tegra194 platforms do not support memory
      mapped timers.

   -  Skips the test "Create all power states and validate EL3 power state parsing"
      from the "EL3 power state parser validation" test suite as it is not in
      sync with this expectation.

   -  Moved reset, timers. wake, watchdog drivers from Tegra194 specific folder to
      common driver folder so that these drivers can be used for other NVIDIA platforms.

-  New tests:

   -  Add test for SDEI RM_ANY routing mode.

   -  Add initial platform support for TC0.

   -  Add SMC fuzzing module test.

   -  Add test case for SMCCC_ARCH_SOC_ID feature.

   -  Add test that supports ARMv8.6-FGT in TF-A.

   -  Add test that supports ARMv8.6-ECV in TF-A.

   -  Add test for FFA_VERSION interface.

   -  Add test for FFA_FEATURES interface.

   -  Add console driver for the TI UART 16550.

   -  Add tests for FF-A memory sharing interfaces between tftf
      and cactus secure partitions.

   -  NVIDIA Tegra194:

      -  Introduce platform port for Tegra194 to to initialize the tftf
         framework and execute tests on the CPUs.

      -  Introduce power management support.

      -  Introduce support for RTC as wake source.

      -  Introduce system reset functionality test.

      -  Introduce watchdog timer test.

      -  Introduce support for NVIDIA Denver CPUs.

      -  Introduce RAS uncorrectable error injection test.

      -  Introduce tests to verify the Video Memory resize interface.

      -  Introduce test to inject RAS corrected errors for all supported
         nodes from all CPUs.

      -  Introduce a test to get return value from SMC SiP function
         TEGRA_SIP_GET_SMMU_PER.

   -  NVIDIA Tegra196:

      -  Introduce initial support for Tegra186 platforms.

   -  NVIDIA Tegra210:

      -  Introduce initial support for Tegra210 platforms.

Secure partition - Cactus
~~~~~~~~~~~~~~~~~~~~~~~~~

-  TFTF doesn't need to boot Secondary Cactus as Hafnium now boots all
   partitions according to "boot-order" field value in the partition
   manifests.

-  Remove test files related to deprecated SPCI Alpha specification and
   SPRT interface.

-  Select different stdout device at runtime as primary VM can access
   to UART while secondary VM's use hypervisor call to SPM for debug
   logging.

-  An SP maps its RX/TX buffers in its EL1&0 Stage-1 translation regime.
   The same RX/TX buffers are mapped by the SPMC in the SP's EL1&0
   Stage-2 translation regime during boot time.

-  Update memory/device region nodes in manifest. Memory region has 3
   entries such as RX buffer, TX buffer and dummy. These memory region
   entries are mapped with attributes as "RX buffer: read-only",
   "TX buffer: read-write" and "dummy: read-write-execute".
   Device region mapped with read-write attribute.

-  Create tertiary partition without RX_TX region specified to test the
   RXTX_MAP API.

-  Add third partition to ffa_partition_info_get test to test that a
   partition can successfully get information about the third cactus
   partition.

-  Map RXTX region to third partition to point the mailbox to this RXTX
   region.

-  Adjust the number of EC context to max number of PEs as per the FF-A
   specification mandating that a SP must either "Implement as many ECs
   as the number of PEs (in case of a "multi-processor" SP with pinned
   contexts)" or "Implement a single EC (in case of a migratable
   "uni-processor" SP).

-  Updated cactus test payload and TFTF ids as it is decided to have
   secure partition FF-A ids in the range from 0x8001 to 0xfffe, 0x8000
   and 0xffff FF-A ids are reserved for the SPMC and the SPMD respectively
   and in the non-secure worlds, FF-A id 0 is reserved for the hypervisor
   and 1 to 0x7fff FF-A ids are reserved for VMs.

-  Break the message loop on bad message request instead of replying
   with the FF-A error ABI to the SPMC.

-  Remove deprecated hypervisor calls spm_vm_get_count and spm_vcpu_get_count.
   Instead use FFA_PARTITION_INFO_GET discovery ABI.

-  Implement hvc call 'SPM_INTERRUPT_GET' to get interrupt id.

-  Re-structure platform dependent files by moving platform dependent files
   and macros to platform specific folder.

-  Adjust partition info get properties to support receipt of direct
   message request.

-  New tests:

   -  Add FFA Version Test.

   -  Add FFA_FEATURES test.

   -  Add FFA_MEM_SHARE test

   -  Add FFA_MEM_LEND test.

   -  Add FFA_MEM_DONATE test.

   -  Add FFA_PARTITION_INFO_GET test.

   -  Add exception/interrupt framework.

   -  Add cactus support for TC0 platform.

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Update link to SMCCC specification.

-  Trim down the top-level readme file to give brief overview of the project
   and also fix/update a number of broken/out-dated links in it.

-  Bug fix in Multicore IRQ spurious test.

-  Fix memory regions mapping with no NS bit set.

-  Reenable PSCI NODE_HW_STATE test which was disabled earlier due to
   outdated SCP firmware.

-  Fix Aarch32 zeromem() function by avoiding infinite loop in 'zeromem'
   function and optimizing 'memcpy4' function.

-  Add missing help_tests info on help target in the top-level Makefile.

-  Trim down the readme file as it does not need to provide detailed
   information, instead it can simply be a landing page providing a brief
   overview of the project and redirecting the reader to RTD for further
   information.

-  Fix maximum number of CPUs in DSU cluster by setting maximum number of CPUs
   in DSU cluster to 8.

Version 2.3
-----------

New features
^^^^^^^^^^^^

-  More tests are made available in this release to help validate
   the functionality of TF-A.

-  CI upgraded to use GCC 9.2-2019.12 toolchain for tf-a-tests.

-  Various improvements to test framework and test suite.

TFTF
~~~~

-  Support for extended register usage as per SMCCC v1.2 specification.

-  Support for FVP platforms with SMT capabilities.

-  Improved support for documentation through addition of basic Sphinx
   configuration and Makefile similar to TF-A repository.

-  Enhancement to libc library synchronous to TF-A code base.

-  ARMv8.3-PAuth enabled for all FWU tests in TFTF.

-  TFTF made RFC 4122 compliant by converting UUIDs to network order format.

-  Build improvement by deprecating custom AARCH64/AARCH32 macros in favor of
   __arch64__  macro provided by compiler.

-  Support for HVC as a SMCCC conduit in TFTF.

-  New tests:

   -  AArch32 tests for checking if PMU counters leak in secure world.

   -  Add new debug filesystem (debugfs) test.

   -  Add a SPCI direct messaging test targeting bare-metal cactus SP.


Secure partitions
~~~~~~~~~~~~~~~~~

Cactus
~~~~~~

-  Several build improvements and symbol relocation fixup to make it position
   independent executable.

-  Update of sample manifest to SPCI Beta1 format.

-  Support for generating JSON file as required by TF-A.

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Makefile bug fix for performing parallel builds.

-  Add missing D-cache invalidation of RW memory in tftf_entrypoint to safeguard
   against possible corruption.

-  Fixes in GIC drivers to support base addresses beyond 4G range.

-  Fix build with XML::LibXML 2.0202 Perl module

Known issues and limitations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The sections below list the known issues and limitations of each test image
provided in this repository. Unless and otherwise stated, issues and limitations
stated in previous release continue to exist in this release.

TFTF
~~~~
-  NODE_HW_STATE test has been temporarily disabled for sgi575 platform due to a
   dependency on SCP binaries version 2.5

Version 2.2
-----------

New features
^^^^^^^^^^^^

-  A wide range of tests are made available in this release to help validate
   the functionality of TF-A.

-  Various improvements to test framework and test suite.

TFTF
~~~~

-  Enhancement to xlat table library synchronous to TF-A code base.

-  Enabled strict alignment checks (SCTLR.A & SCTLR.SA) in all images.

-  Support for a simple console driver. Currently it serves as a placeholder
   with empty functions.

-  A topology helper API is added in the framework to get parent node info.

-  Support for FVP with clusters having upto 8 CPUs.

-  Enhanced linker script to separate code and RO data sections.

-  Relax SMC calls tests. The SMCCC specification recommends Trusted OSes to
   mitigate the risk of leaking information by either preserving the register
   state over the call, or returning a constant value, such as zero, in each
   register. Tests only allowed the former behaviour and have been extended to
   allow the latter as well.

-  Pointer Authentication enabled on warm boot path with individual APIAKey
   generation for each CPU.

-  New tests:

   -  Basic unit tests for xlat table library v2.

   -  Tests for validating SVE support in TF-A.

   -  Stress tests for dynamic xlat table library.

   -  PSCI test to measure latencies when turning ON a cluster.

   -  Series of AArch64 tests that stress the secure world to leak sensitive
      counter values.

   -  Test to validate PSCI SYSTEM_RESET call.

   -  Basic tests to validate Memory Tagging Extensions are being enabled and
      ensuring no undesired leak of sensitive data occurs.

-  Enhanced tests:

   -  Improved tests for Pointer Authentication support. Checks are performed
      to see if pointer authentication keys are accessible as well as validate
      if secure keys are being leaked after a PSCI version call or TSP call.

   -  Improved AMU test to remove unexecuted code iterating over Group1 counters
      and fix the conditional check of AMU Group0 counter value.

Secure partitions
~~~~~~~~~~~~~~~~~

A new Secure Partition Quark is introduced in this release.

Quark
~~~~~

The Quark test secure partition provided is a simple service which returns a
magic number. Further, a simple test is added to test if Quark is functional.

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Bug fix in libc memchr implementation.

-  Bug fix in calculation of number of CPUs.

-  Streamlined SMC WORKAROUND_2 test and fixed a false fail on Cortex-A76 CPU.

-  Pointer Authentication support is now available for secondary CPUs and the
   corresponding tests are stable in this release.

Known issues and limitations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The sections below list the known issues and limitations of each test image
provided in this repository. Unless and otherwise stated, issues and limitations
stated in previous release continue to exist in this release.

TFTF
~~~~
-  Multicore spurious interrupt test is observed to have unstable behavior. As a
   temporary solution, this test is skipped for AArch64 Juno configurations.

-  Generating SVE instructions requires `O3` compilation optimization. Since the
   current build structure does not allow compilation flag modification for
   specific files, the function which tests support for SVE has been pre-compiled
   and added as an assembly file.



Version 2.1
-----------

New features
^^^^^^^^^^^^

-  Add initial support for testing Secure Partition Client Interface (SPCI)
   and Secure Partition Run-Time (SPRT) standards.

   Exercise the full communication flow throughout the software stack, involving:

   -  A Secure-EL0 test partition as the Trusted World agent.

   -  TFTF as the Normal World agent.

   -  The Secure Partition Manager (SPM) in TF-A.

-  Various stability improvements, code refactoring and clean ups.

TFTF
~~~~

-  Reorganize tests build infrastructure to allow the selection of a subset of
   tests.

-  Reorganize the platform layer for improved clarity and simplicity.

-  Sanitise inclusion of drivers header files.

-  Enhance the test report format for improved clarity and conciseness.

-  Dump CPU registers when hitting an unexpected exception. Previously, this
   would silently loop forever.

-  Import libc from TF-A to better align the two code bases.

-  New tests:

   -  SPM tests for exercising communication through either the MM or SPCI/SPRT
      interfaces.

   -  SMC calling convention tests.

   -  Initial tests for Armv8.3 Pointer Authentication support (experimental).

-  New platform ports:

   - `Arm SGI-575`_  FVP.

   - Hikey960 board (experimental).

   - `Arm Neoverse Reference Design N1 Edge (RD-N1-Edge)`_ FVP (experimental).

Secure partitions
~~~~~~~~~~~~~~~~~

We now have 3 Secure Partitions to test the SPM implementation in TF-A.

Cactus-MM
'''''''''

The Cactus test secure partition provided in version 2.0 has been renamed into
"*Cactus-MM*". It is still responsible for testing the SPM implementation based
on the Arm Management Mode Interface.

Cactus
''''''

This is a new test secure partition (as the former "*Cactus*" has been renamed
into "*Cactus-MM*", see above).

Unlike *Cactus-MM*, this image tests the SPM implementation based on the SPCI
and SPRT draft specifications.

It runs in Secure-EL0 and performs the following tasks:

-  Test that TF-A has correctly setup the secure partition environment (access
   to cache maintenance operations, to floating point registers, etc.)

-  Test that TF-A accepts to change data access permissions and instruction
   permissions on behalf of Cactus for memory regions the latter owns.

-  Test communication with SPM through SPCI/SPRT interfaces.

Ivy
'''

This is also a new test secure partition. It is provided in order to test
multiple partitions support in TF-A. It is derived from Cactus and essentially
provides the same services but with different identifiers at the moment.

EL3 payload
~~~~~~~~~~~

-  New platform ports:

   - `Arm SGI-575`_  FVP.

   - `Arm Neoverse Reference Design N1 Edge (RD-N1-Edge)`_ FVP (experimental).

Issues resolved since last release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  The GICv2 spurious IRQ test is no longer Juno-specific. It is now only
   GICv2-specific.

-  The manual tests in AArch32 state now work properly. After investigation,
   we identified that this issue was not AArch32 specific but concerned any
   test relying on state information persisting across reboots. It was due to
   an incorrect build configuration.

-  Cactus-MM now successfully links with GNU toolchain 7.3.1.

Known issues and limitations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The sections below lists the known issues and limitations of each test image
provided in this repository.

TFTF
~~~~

The TFTF test image might be conceptually sub-divided further in 2 parts: the
tests themselves, and the test framework they are based upon.

Test framework
~~~~~~~~~~~~~~

-  Some stability issues.

-  No mechanism to abort tests when they time out (e.g. this could be
   implemented using a watchdog).

-  No convenient way to include or exclude tests on a per-platform basis.

-  Power domains and affinity levels are considered equivalent but they may
   not necessarily be.

-  Need to provide better support to alleviate duplication of test code. There
   are some recurrent test patterns for which helper functions should be
   provided. For example, bringing up all CPUs on the platform and executing the
   same function on all of them, or programming an interrupt and waiting for it
   to trigger.

-  Every CPU that participates in a test must return from the test function. If
   it does not - e.g. because it powered itself off for testing purposes - then
   the test framework will wait forever for this CPU. This limitation is too
   restrictive for some tests.

-  No protection against interrupted flash operations. If the target is reset
   while some data is written to flash, the test framework might behave
   incorrectly on reset.

-  When compiling the code, if the generation of the ``tests_list.c`` and/or
   ``tests_list.h`` files fails, the build process is not aborted immediately
   and will only fail later on.

-  The directory layout requires further improvements. Most of the test
   framework code has been moved under the ``tftf/`` directory to better isolate
   it but this effort is not complete. As a result, there are still some TFTF
   files scattered around.

-  Pointer Authentication testing is experimental and incomplete at this stage.
   It is only enabled on the primary CPU on the cold boot.

Tests
~~~~~

-  Some tests are implemented for AArch64 only and are skipped on AArch32.

-  Some tests are not robust enough:

   -  Some tests might hang in some circumstances. For example, they might wait
      forever for a condition to become true.

   -  Some tests rely on arbitrary time delays instead of proper synchronization
      when executing order-sensitive steps.

   -  Some tests have been implemented in a practical manner: they seem to work
      on actual hardware but they make assumptions that are not guaranteed by
      the Arm architecture. Therefore, they might fail on some other platforms.

-  PSCI stress tests are very unreliable and will often hang. The root cause is
   not known for sure but this might be due to bad synchronization between CPUs.

-  The GICv2 spurious IRQ test sometimes fails with the following error message:

   ``SMC @ lead CPU returned 0xFFFFFFFF 0x8 0xC``

   The root cause is unknown.

-  The FWU tests take a long time to complete. This is because they wait for the
   watchdog to reset the system. On FVP, TF-A configures the watchdog period to
   about 4 min. This limit is excessive for an automated testing context and
   leaves the user without feedback and unable to determine if the tests are
   proceeding properly.

-  The test "Target timer to a power down cpu" sometimes fails with the
   following error message:

   ``Expected timer switch: 4 Actual: 3``

   The root cause is unknown.

FWU images
~~~~~~~~~~

-  The FWU tests do not work on the revC of the Base AEM FVP. They only work on
   the revB.

-  NS-BL1U and NS-BL2U images reuse TFTF-specific code for legacy reasons. This
   is not a clean design and may cause confusion.

Test secure partitions (Cactus, Cactus-MM, Ivy)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  This is experimental code. It's likely to change a lot as the secure
   partition software architecture evolves.

-  Supported on AArch64 FVP platform only.

All test images
~~~~~~~~~~~~~~~

-  TF-A Tests are derived from a fork of TF-A so:

    -  they've got some code in common but lag behind on some features.

    -  there might still be some irrelevant references to TF-A.

-  Some design issues.
   E.g. TF-A Tests inherited from the I/O layer of TF-A, which still needs a
   major rework.

-  Cannot build TF-A Tests with Clang. Only GCC is supported.

-  The build system does not cope well with parallel building. The user should
   not attempt to run multiple jobs in parallel with the ``-j`` option of `GNU
   make`.

-  The build system does not properly track build options. A clean build must be
   performed every time a build option changes.

-  UUIDs are not compliant to RFC 4122.

-  No floating point support. The code is compiled with GCC flag
   ``-mgeneral-regs-only``, which prevents the compiler from generating code
   that accesses floating point registers. This might limit some test scenarios.

-  The documentation is too lightweight.

-  Missing instruction barriers in some places before reading the system counter
   value. As a result, the CPU could speculatively read it and any delay loop
   calculations might be off (because based on stale values). We need to examine
   all such direct reads of the ``CNTPCT_EL0`` register and replace them with a
   call to ``syscounter_read()`` where appropriate.

Version 2.0
-----------

New features
^^^^^^^^^^^^

This is the first public release of the Trusted Firmware-A Tests source code.

TFTF
~~~~

-  Provides a baremetal test framework to exercise TF-A features through its
   ``SMC`` interface.

-  Integrates easily with TF-A: the TFTF binary is packaged in the FIP image
   as a ``BL33`` component.

-  Standalone binary that runs on the target without human intervention (except
   for some specific tests that require a manual target reset).

-  Designed for multi-core testing. The various sub-frameworks allow maximum
   parallelism in order to stress the firmware.

-  Displays test results on the UART output. This may then be parsed by an
   external tool and integrated in a continuous integration system.

-  Supports running in AArch64 (NS-EL2 or NS-EL1) and AArch32 states.

-  Supports parsing a tests manifest (XML file) listing the tests to include in
   the binary.

-  Detects most platform features at run time (e.g. topology, GIC version, ...).

-  Provides a topology enumeration framework. Allows tests to easily go through
   affinity levels and power domain nodes.

-  Provides an event framework to synchronize CPU operations in a multi-core
   context.

-  Provides a timer framework. Relies on a single global timer to generate
   interrupts for all CPUs in the system. This allows tests to easily program
   interrupts on demand to use as a wake-up event source to come out of CPU
   suspend state for example.

-  Provides a power-state enumeration framework. Abstracts the valid power
   states supported on the platform.

-  Provides helper functions for power management operations (CPU hotplug,
   CPU suspend, system suspend, ...) with proper saving of the hardware state.

-  Supports rebooting the platform at the end of each test for greater
   independence between tests.

-  Supports interrupting and resuming a test session. This relies on storing
   test results in non-volatile memory (e.g. flash).

FWU images
~~~~~~~~~~

-  Provides example code to exercise the Firmware Update feature of TF-A.

-  Tests the robustness of the FWU state machine implemented in the TF-A by
   sending valid and invalid authentication, copy and image execution requests
   to the TF-A BL1 image.

EL3 test payload
~~~~~~~~~~~~~~~~

-  Tests the ability of TF-A to load an EL3 payload.

Cactus test secure partition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  Tests that TF-A has correctly setup the secure partition environment: it
   should be allowed to perform cache maintenance operations, access floating
   point registers, etc.

-  Tests the ability of a secure partition to request changing data access
   permissions and instruction permissions of memory regions it owns.

-  Tests the ability of a secure partition to handle StandaloneMM requests.

Known issues and limitations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The sections below lists the known issues and limitations of each test image
provided in this repository.

TFTF
~~~~

The TFTF test image might be conceptually sub-divided further in 2 parts: the
tests themselves, and the test framework they are based upon.

Test framework
~~~~~~~~~~~~~~

-  Some stability issues.

-  No mechanism to abort tests when they time out (e.g. this could be
   implemented using a watchdog).

-  No convenient way to include or exclude tests on a per-platform basis.

-  Power domains and affinity levels are considered equivalent but they may
   not necessarily be.

-  Need to provide better support to alleviate duplication of test code. There
   are some recurrent test patterns for which helper functions should be
   provided. For example, bringing up all CPUs on the platform and executing the
   same function on all of them, or programming an interrupt and waiting for it
   to trigger.

-  Every CPU that participates in a test must return from the test function. If
   it does not - e.g. because it powered itself off for testing purposes - then
   the test framework will wait forever for this CPU. This limitation is too
   restrictive for some tests.

-  No protection against interrupted flash operations. If the target is reset
   while some data is written to flash, the test framework might behave
   incorrectly on reset.

-  When compiling the code, if the generation of the tests_list.c and/or
   tests_list.h files fails, the build process is not aborted immediately and
   will only fail later on.

-  The directory layout is confusing. Most of the test framework code has been
   moved under the ``tftf/`` directory to better isolate it but this effort is
   not complete. As a result, there are still some TFTF files scattered around.

Tests
~~~~~

-  Some tests are implemented for AArch64 only and are skipped on AArch32.

-  Some tests are not robust enough:

   -  Some tests might hang in some circumstances. For example, they might wait
      forever for a condition to become true.

   -  Some tests rely on arbitrary time delays instead of proper synchronization
      when executing order-sensitive steps.

   -  Some tests have been implemented in a practical manner: they seem to work
      on actual hardware but they make assumptions that are not guaranteed by
      the Arm architecture. Therefore, they might fail on some other platforms.

-  PSCI stress tests are very unreliable and will often hang. The root cause is
   not known for sure but this might be due to bad synchronization between CPUs.

-  The GICv2 spurious IRQ test is Juno-specific. In reality, it should only be
   GICv2-specific. It should be reworked to remove any platform-specific
   assumption.

-  The GICv2 spurious IRQ test sometimes fails with the following error message:

   ``SMC @ lead CPU returned 0xFFFFFFFF 0x8 0xC``

   The root cause is unknown.

-  The manual tests in AArch32 mode do not work properly. They save some state
   information into non-volatile memory in order to detect the reset reason but
   this state does not appear to be retained. As a result, these tests keep
   resetting infinitely.

-  The FWU tests take a long time to complete. This is because they wait for the
   watchdog to reset the system. On FVP, TF-A configures the watchdog period to
   about 4 min. This is way too long in an automated testing context. Besides,
   the user gets not feedback, which may let them think that the tests are not
   working properly.

-  The test "Target timer to a power down cpu" sometimes fails with the
   following error message:

   ``Expected timer switch: 4 Actual: 3``

   The root cause is unknown.

FWU images
~~~~~~~~~~

-  The FWU tests do not work on the revC of the Base AEM FVP. They only work on
   the revB.

-  NS-BL1U and NS-BL2U images reuse TFTF-specific code for legacy reasons. This
   is not a clean design and may cause confusion.

Cactus test secure partition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  Cactus is experimental code. It's likely to change a lot as the secure
   partition software architecture evolves.

-  Fails to link with GNU toolchain 7.3.1.

-  Cactus is supported on AArch64 FVP platform only.

All test images
~~~~~~~~~~~~~~~

-  TF-A Tests are derived from a fork of TF-A so:

    -  they've got some code in common but lag behind on some features.

    -  there might still be some irrelevant references to TF-A.

-  Some design issues.
   E.g. TF-A Tests inherited from the I/O layer of TF-A, which still needs a
   major rework.

-  Cannot build TF-A Tests with Clang. Only GCC is supported.

-  The build system does not cope well with parallel building. The user should
   not attempt to run multiple jobs in parallel with the ``-j`` option of `GNU
   make`.

-  The build system does not properly track build options. A clean build must be
   performed every time a build option changes.

-  SMCCC v2 is not properly supported.

-  UUIDs are not compliant to RFC 4122.

-  No floating point support. The code is compiled with GCC flag
   ``-mgeneral-regs-only``, which prevents the compiler from generating code
   that accesses floating point registers. This might limit some test scenarios.

-  The documentation is too lightweight.

--------------

*Copyright (c) 2018-2022, Arm Limited. All rights reserved.*

.. _Arm Neoverse Reference Design N1 Edge (RD-N1-Edge): https://developer.arm.com/products/system-design/reference-design/neoverse-reference-design
.. _Arm SGI-575: https://developer.arm.com/products/system-design/fixed-virtual-platforms
