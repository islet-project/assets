
.. section-numbering::
    :suffix: .

.. contents::

Please note that the Trusted Firmware-A Tests version follows the Trusted
Firmware-A version for simplicity. At any point in time, TF-A Tests version
`x.y` aims at testing TF-A version `x.y`. Different versions of TF-A and TF-A
Tests are not guaranteed to be compatible. This also means that a version
upgrade on the TF-A-Tests side might not necessarily introduce any new feature.

Trusted Firmware-A Tests - version 2.2
======================================

New features
------------

-  A wide range of tests are made available in this release to help validate
   the functionality of TF-A.

-  Various improvements to test framework and test suite.

TFTF
````

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
`````````````````

A new Secure Partition Quark is introduced in this release.

Quark
'''''''''

The Quark test secure partition provided is a simple service which returns a
magic number. Further, a simple test is added to test if Quark is functional.

Issues resolved since last release
----------------------------------

-  Bug fix in libc memchr implementation.

-  Bug fix in calculation of number of CPUs.

-  Streamlined SMC WORKAROUND_2 test and fixed a false fail on Cortex-A76 CPU.

-  Pointer Authentication support is now available for secondary CPUs and the
   corresponding tests are stable in this release.

Known issues and limitations
----------------------------

The sections below list the known issues and limitations of each test image
provided in this repository. Unless and otherwise stated, issues and limitations
stated in previous release continue to exist in this release.

TFTF
````

Tests
'''''

-  Multicore spurious interrupt test is observed to have unstable behavior. As a
   temporary solution, this test is skipped for AArch64 Juno configurations.

-  Generating SVE instructions requires `O3` compilation optimization. Since the
   current build structure does not allow compilation flag modification for
   specific files, the function which tests support for SVE has been pre-compiled
   and added as an assembly file.



Trusted Firmware-A Tests - version 2.1
======================================

New features
------------

-  Add initial support for testing Secure Partition Client Interface (SPCI)
   and Secure Partition Run-Time (SPRT) standards.

   Exercise the full communication flow throughout the software stack, involving:

   -  A Secure-EL0 test partition as the Trusted World agent.

   -  TFTF as the Normal World agent.

   -  The Secure Partition Manager (SPM) in TF-A.

-  Various stability improvements, code refactoring and clean ups.

TFTF
````

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
`````````````````

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
```````````

-  New platform ports:

   - `Arm SGI-575`_  FVP.

   - `Arm Neoverse Reference Design N1 Edge (RD-N1-Edge)`_ FVP (experimental).

Issues resolved since last release
----------------------------------

-  The GICv2 spurious IRQ test is no longer Juno-specific. It is now only
   GICv2-specific.

-  The manual tests in AArch32 state now work properly. After investigation,
   we identified that this issue was not AArch32 specific but concerned any
   test relying on state information persisting across reboots. It was due to
   an incorrect build configuration.

-  Cactus-MM now successfully links with GNU toolchain 7.3.1.

Known issues and limitations
----------------------------

The sections below lists the known issues and limitations of each test image
provided in this repository.

TFTF
````

The TFTF test image might be conceptually sub-divided further in 2 parts: the
tests themselves, and the test framework they are based upon.

Test framework
''''''''''''''

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
'''''

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
``````````

-  The FWU tests do not work on the revC of the Base AEM FVP. They only work on
   the revB.

-  NS-BL1U and NS-BL2U images reuse TFTF-specific code for legacy reasons. This
   is not a clean design and may cause confusion.

Test secure partitions (Cactus, Cactus-MM, Ivy)
```````````````````````````````````````````````

-  This is experimental code. It's likely to change a lot as the secure
   partition software architecture evolves.

-  Supported on AArch64 FVP platform only.

All test images
```````````````

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

Trusted Firmware-A Tests - version 2.0
======================================

New features
------------

This is the first public release of the Trusted Firmware-A Tests source code.

TFTF
````

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
``````````

-  Provides example code to exercise the Firmware Update feature of TF-A.

-  Tests the robustness of the FWU state machine implemented in the TF-A by
   sending valid and invalid authentication, copy and image execution requests
   to the TF-A BL1 image.

EL3 test payload
````````````````

-  Tests the ability of TF-A to load an EL3 payload.

Cactus test secure partition
````````````````````````````

-  Tests that TF-A has correctly setup the secure partition environment: it
   should be allowed to perform cache maintenance operations, access floating
   point registers, etc.

-  Tests the ability of a secure partition to request changing data access
   permissions and instruction permissions of memory regions it owns.

-  Tests the ability of a secure partition to handle StandaloneMM requests.

Known issues and limitations
----------------------------

The sections below lists the known issues and limitations of each test image
provided in this repository.

TFTF
````

The TFTF test image might be conceptually sub-divided further in 2 parts: the
tests themselves, and the test framework they are based upon.

Test framework
''''''''''''''

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
'''''

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
``````````

-  The FWU tests do not work on the revC of the Base AEM FVP. They only work on
   the revB.

-  NS-BL1U and NS-BL2U images reuse TFTF-specific code for legacy reasons. This
   is not a clean design and may cause confusion.

Cactus test secure partition
````````````````````````````

-  Cactus is experimental code. It's likely to change a lot as the secure
   partition software architecture evolves.

-  Fails to link with GNU toolchain 7.3.1.

-  Cactus is supported on AArch64 FVP platform only.

All test images
```````````````

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

*Copyright (c) 2018-2019, Arm Limited. All rights reserved.*

.. _Arm Neoverse Reference Design N1 Edge (RD-N1-Edge): https://developer.arm.com/products/system-design/reference-design/neoverse-reference-design
.. _Arm SGI-575: https://developer.arm.com/products/system-design/fixed-virtual-platforms
