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
-  `EL3 payload`_ boot flow
-  `Secure partition`_ support

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

*Copyright (c) 2019, Arm Limited. All rights reserved.*
