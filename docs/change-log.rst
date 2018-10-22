
.. section-numbering::
    :suffix: .

.. contents::

Please note that the Trusted Firmware-A Tests version follows the Trusted
Firmware-A version for simplicity. At any point in time, TF-A Tests version
`x.y` aims at testing TF-A version `x.y`. Different versions of TF-A and TF-A
Tests are not guaranteed to be compatible. This also means that a version
upgrade on the TF-A-Tests side might not necessarily introduce any new feature.

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

*Copyright (c) 2018, Arm Limited. All rights reserved.*
