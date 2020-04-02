Trusted Firmware-A Tests Documentation
======================================

.. toctree::
   :maxdepth: 1
   :hidden:

   Home<self>
   about/index
   getting_started/index
   process/index
   design
   implementing-tests
   porting/index
   change-log
   license

The Trusted Firmware-A Tests (TF-A-Tests) is a suite of baremetal tests to
exercise the `Trusted Firmware-A (TF-A)`_ features from the Normal World. It
enables strong TF-A functional testing without dependency on a Rich OS. It
mainly interacts with TF-A through its SMC interface.

It provides a basis for TF-A developers to validate their own platform ports and
add their own test cases.

The following TF-A features are currently tested to some extent (this list is
not exhaustive):

-  `SMC Calling Convention`_
-  `Power State Coordination Interface (PSCI)`_
-  `Software Delegated Exception Interface (SDEI)`_
-  `Performance Measurement Framework (PMF)`_
-  Communication and interaction with the `Test Secure Payload (TSP)`_
-  `Firmware update`_ (or recovery mode)
-  `EL3 payload`_ boot flow
-  `Secure partition`_ support

These tests are not a compliance test suite for the Arm interface standards used
in TF-A (such as PSCI).

They do not cover 100% of the TF-A code. The fact that all tests pass does not
mean that TF-A is free of bugs.

They are not reference code. They should not be considered as the official way
to test hardware/firmware features. Instead, they are provided as example code
to experiment with and improve on.

Getting started
---------------

Get the TF-A Tests source code from `trustedfirmware.org`_.

See the `User Guide`_ for instructions on how to install, build and use the TF-A
Tests.

See the `Design Guide`_ for information on how the TF-A Tests internally work.

See the `Porting Guide`_ for information about how to use this software on
another Armv8-A platform.

See the `Contributing Guidelines`_ for information on how to contribute to this
project.

--------------

*Copyright (c)2019, Arm Limited. All rights reserved.*

.. _Juno Arm Development Platform: https://developer.arm.com/products/system-design/development-boards/juno-development-board

.. _Power State Coordination Interface (PSCI): PSCI_
.. _PSCI: http://infocenter.arm.com/help/topic/com.arm.doc.den0022d/Power_State_Coordination_Interface_PDD_v1_1_DEN0022D.pdf
.. _Software Delegated Exception Interface (SDEI): SDEI_
.. _SDEI: http://infocenter.arm.com/help/topic/com.arm.doc.den0054a/ARM_DEN0054A_Software_Delegated_Exception_Interface.pdf
.. _SMC Calling Convention: http://infocenter.arm.com/help/topic/com.arm.doc.den0028b/ARM_DEN0028B_SMC_Calling_Convention.pdf

.. _trustedfirmware.org: https://git.trustedfirmware.org/TF-A/tf-a-tests.git

.. _Trusted Firmware-A (TF-A): TF-A_
.. _TF-A: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about
.. _Test Secure Payload (TSP): TSP_
.. _TSP: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/tree/bl32/tsp
.. _Performance Measurement Framework (PMF): PMF_
.. _PMF: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/firmware-design.rst#performance-measurement-framework
.. _Firmware update: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/firmware-update.rst
.. _EL3 payload: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/user-guide.rst#el3-payloads-alternative-boot-flow
.. _Secure partition: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/secure-partition-manager-design.rst
