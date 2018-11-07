Trusted Firmware-A Tests - Design
=================================

.. section-numbering::
    :suffix: .

.. contents::

This document provides some details about the internals of the TF-A Tests
design. It is incomplete at the moment.

Global overview of the TF-A tests behaviour
-------------------------------------------

The EL3 firmware is expected to hand over to the TF-A tests with all secondary
cores powered down, i.e. only the primary core should enter the TF-A tests.

The primary CPU initialises the platform and the TF-A tests internal data
structures.

Then the test session begins. The TF-A tests are executed one after the
other. Tests results are saved in non-volatile memory as we go along.

Once all tests have completed, a report is printed over the serial console.

Global Code Structure
---------------------

The code is organised into the following categories (present as directories at
the top level or under the ``tftf/`` directory):

-  **Drivers.**

   Some examples follow, this list might not be exhaustive.

   -  Generic GIC driver.

      ``arm_gic.h`` contains the public APIs that tests might use. Both GIC
      architecture versions 2 and 3 are supported.

   -  PL011 UART driver.

   -  VExpress NOR flash driver.

      Note that tests are not expected to use this driver in most
      cases. Instead, they should use the ``tftf_nvm_read()`` and
      ``tftf_nvm_write()`` wrapper APIs. See definitions in
      ``tftf/framework/include/nvm.h``. See also the NVM validation test cases
      (``tftf/tests/framework_validation_tests/test_validation_nvm.c``) for an
      example of usage of these functions.

   -  SP805 watchdog.

      Used solely to generate an interrupt that will reset the system on purpose
      (used in ``tftf_plat_reset()``).

   -  SP804 timer.

      This is used as the system timer on Juno. It is configured such that an
      interrupt is generated when it reaches 0. It is programmed in one-shot
      mode, i.e. it must be rearmed every time it reaches 0.

-  **Framework.**

   Core features of the test framework.

-  **Library code.**

   Firstly, there is ``include/stdlib/`` which provides standard C library
   functions like ``memcpy()``, ``printf()`` and so on.
   Additionally, various other APIs are provided under ``include/lib/``. The
   below list gives some examples but might not be exhaustive.

   -  ``aarch64/``

      Architecture helper functions for e.g. system registers access, cache
      maintenance operations, MMU configuration, ...

   -  ``events.h``

      Events API. Used to create synchronisation points between CPUs in tests.

   -  ``irq.h``

      IRQ handling support. Used to configure IRQs and register/unregister
      handlers called upon reception of a specific IRQ.

   -  ``power_management.h``

      Power management operations (CPU ON/OFF, CPU suspend, etc.).

   -  ``sgi.h``

      Software Generated Interrupt support. Used as an inter-CPU communication
      mechanism.

   -  ``spinlock.h``

      Lightweight implementation of synchronisation locks. Used to prevent
      concurrent accesses to shared data structures.

   -  ``timer.h``

      Support for programming the timer. Any timer which is in the `always-on`
      power domain can be used to exit CPUs from suspend state.

   -  ``tftf_lib.h``

      Miscellaneous helper functions/macros: MP-safe ``printf()``, low-level
      PSCI wrappers, insertion of delays, raw SMC interface, support for writing
      a string in the test report, macros to skip tests on platforms that do not
      meet topology requirements, etc.

   -  ``io_storage.h``

      Low-level IO operations. Tests are not expected to use these APIs
      directly. They should use higher-level APIs like ``tftf_nvm_read()``
      and ``tftf_nvm_write()``.

-  **Platform specific.**

   Note that ``include/plat/common/plat_topology.h`` provides the interfaces
   that a platform must implement to support topology discovery (i.e. how many
   CPUs and clusters there are).

-  **Tests.**

   The tests are divided into the following categories (present as directories in
   the ``tftf/tests/`` directory):

   -  **Framework validation tests.**

      Tests that exercise the core features of the framework. Verify that the test
      framework itself works properly.

   -  **Runtime services tests.**

      Tests that exercise the runtime services offered by the EL3 Firmware to the
      Normal World software. For example, this includes tests for the Standard
      Service (to which PSCI belongs to), the Trusted OS service or the SiP
      service.

   -  **CPU extensions tests.**

      Tests some CPU extensions features. For example, the AMU tests ensure that
      the counters provided by the Activity Monitor Unit are behaving correctly.

   -  **Firmware Update tests.**

      Tests that exercise the `Firmware Update`_ feature of TF-A.

   -  **Template tests.**

      Sample test code showing how to write tests in practice. Serves as
      documentation.

   -  **Performance tests.**

      Simple tests measuring the latency of an SMC call.

   -  **Miscellaneous tests.**

      Tests for RAS support, correct system setup, ...

All assembler files have the ``.S`` extension. The linker source file has the
extension ``.ld.S``. This is processed by GCC to create the linker script which
has the extension ``.ld``.

Detailed Code Structure
-----------------------

The cold boot entry point is ``tftf_entrypoint`` (see
``tftf/framework/aarch64/entrypoint.S``). As explained in section `Global
overview of the TF-A tests behaviour`_, only the primary CPU is expected to
execute this code.

Tests can power on other CPUs using the function ``tftf_cpu_on()``. This uses
the PSCI ``CPU_ON`` API of the EL3 Firmware. When entering the Normal World,
execution starts at the warm boot entry point, which is ``tftf_hotplug_entry()``
(see ``tftf/framework/aarch64/entrypoint.S``).

Information about the progression of the test session and tests results are
written into Non-Volatile Memory as we go along. This consists of the following
data (see struct ``tftf_state_t`` typedef in ``tftf/framework/include/nvm.h``):

-   ``test_to_run``

    Reference to the test to run.

-   ``test_progress``

    Progress in the execution of ``test_to_run``. This is used to implement the
    following state machine:

::

   +-> TEST_READY (initial state of the test)                  <--------------+
   |        |                                                                 |
   |        |  Test framework prepares the test environment.                  |
   |        |                                                                 |
   |        v                                                                 |
   |   TEST_IN_PROGRESS                                                       |
   |        |                                                                 |
   |        |  Hand over to the test function.                                |
   |        |  If the test wants to reboot the platform  ---> TEST_REBOOTING  |
   |        |                                                       |         |
   |        |  Test function returns into framework.                | Reboot  |
   |        |                                                       |         |
   |        |                                                       +---------+
   |        v
   |   TEST_COMPLETE
   |        |
   |        |  Do some framework management.
   |        |  Move to next test.
   +--------+

-   ``testcase_buffer``

    A buffer that the test can use as a scratch area for whatever it is doing.

-   ``testcase_results``

-   ``result_buffer_size``

-   ``result_buffer``

    Buffer holding the tests output. Tests output are concatenated.

Interrupts management
---------------------

The TF-A tests expect SGIs #0 to #7 to be available for their own usage. In
particular, this means that Trusted World software must configure them as
non-secure interrupts.

SGI #7 has a special status. It is the SGI that the timer management framework
sends to all CPUs when the system timer fires off (see the definition of the
constant ``IRQ_WAKE_SGI`` in the header file ``include/lib/irq.h``). Although
test cases can use this specific SGI - e.g. they can register an IRQ handler for
it and use it as an inter-CPU communication mechanism - they have to be aware of
the underlying consequences. Some tests, like the PSCI ``CPU_SUSPEND`` tests,
rely on this SGI to be enabled in order to wake up CPUs from their suspend
state. If it is disabled, these tests will leave the system in an unresponsive
state.

--------------

*Copyright (c) 2018, Arm Limited. All rights reserved.*

.. _Summary of build options: user-guide.rst#summary-of-build-options
.. _Firmware Update: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/firmware-update.rst
