Trusted Firmware-A Tests
========================

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

More Info and Documentation
---------------------------

To find out more about Trusted Firmware-A Tests, please
`view the full documentation`_ that is available through `trustedfirmware.org`_.

--------------

*Copyright (c) 2018-2020, Arm Limited. All rights reserved.*

.. _Power State Coordination Interface (PSCI): PSCI_
.. _PSCI: http://infocenter.arm.com/help/topic/com.arm.doc.den0022d/Power_State_Coordination_Interface_PDD_v1_1_DEN0022D.pdf

.. _Software Delegated Exception Interface (SDEI): SDEI_
.. _SDEI: http://infocenter.arm.com/help/topic/com.arm.doc.den0054a/ARM_DEN0054A_Software_Delegated_Exception_Interface.pdf

.. _SMC Calling Convention: https://developer.arm.com/docs/den0028/latest

.. _Trusted Firmware-A (TF-A): TF-A_
.. _TF-A: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git

.. _Test Secure Payload (TSP): TSP_
.. _TSP: https://trustedfirmware-a.readthedocs.io/en/latest/perf/tsp.html

.. _Performance Measurement Framework (PMF): PMF_
.. _PMF: https://trustedfirmware-a.readthedocs.io/en/latest/design/firmware-design.html#performance-measurement-framework

.. _Firmware update: https://trustedfirmware-a.readthedocs.io/en/latest/components/firmware-update.html
.. _EL3 payload: https://trustedfirmware-a.readthedocs.io/en/latest/design/alt-boot-flows.html#el3-payloads-alternative-boot-flow
.. _Secure partition: https://trustedfirmware-a.readthedocs.io/en/latest/components/secure-partition-manager-design.html

.. _view the full documentation: https://trustedfirmware-a-tests.readthedocs.io/
.. _trustedfirmware.org: http://www.trustedfirmware.org
