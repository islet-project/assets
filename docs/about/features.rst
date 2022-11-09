Feature Overview
================

This page provides an overview of the current TF-A Tests feature set. The
:ref:`Change Log & Release Notes` document provides details of changes made
since the last release.

Current Features
----------------

The following TF-A features are currently tested to some extent (this list is
not exhaustive):

-  `SMC Calling Convention`_
-  `Power State Coordination Interface (PSCI)`_
-  `Software Delegated Exception Interface (SDEI)`_
-  `Performance Measurement Framework (PMF)`_
-  Communication and interaction with the `Test Secure Payload (TSP)`_
-  `Firmware update`_ (or recovery mode)
-  `EL3 payload boot flow`_
-  Secure partition support
-  `True Random Number Generator Firmware Interface (TRNG_FW)`_

These tests are not a compliance test suite for the Arm interface standards used
in TF-A (such as PSCI).

They do not cover 100% of the TF-A code. The fact that all tests pass does not
mean that TF-A is free of bugs.

They are not reference code. They should not be considered as the official way
to test hardware/firmware features. Instead, they are provided as example code
to experiment with and improve on.

Still to come
-------------

-  Additional tests
-  Support for new platforms
-  Design improvements
-  Stability improvements
-  Enhancements to the test framework to make it easier to implement tests
-  Fixes for known issues (detailed in :ref:`Change Log & Release Notes`)

--------------

*Copyright (c) 2019-2020, Arm Limited. All rights reserved.*

.. _SMC Calling Convention: https://developer.arm.com/docs/den0028/latest
.. _Power State Coordination Interface (PSCI): PSCI_
.. _PSCI: http://infocenter.arm.com/help/topic/com.arm.doc.den0022d/Power_State_Coordination_Interface_PDD_v1_1_DEN0022D.pdf
.. _Software Delegated Exception Interface (SDEI): SDEI_
.. _SDEI: http://infocenter.arm.com/help/topic/com.arm.doc.den0054a/ARM_DEN0054A_Software_Delegated_Exception_Interface.pdf
.. _Performance Measurement framework (PMF): PMF_
.. _PMF: https://trustedfirmware-a.readthedocs.io/en/latest/design/firmware-design.html#performance-measurement-framework
.. _Test Secure Payload (TSP): TSP_
.. _TSP: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/tree/bl32/tsp
.. _Firmware update: https://trustedfirmware-a.readthedocs.io/en/latest/components/firmware-update.html
.. _EL3 payload boot flow: https://trustedfirmware-a.readthedocs.io/en/latest/design/alt-boot-flows.html#el3-payloads-alternative-boot-flow
.. _TRNG_FW: https://developer.arm.com/documentation/den0098/latest
