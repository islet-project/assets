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

Getting started
---------------

Get the TF-A Tests source code from `trustedfirmware.org`_.

See content under the *Getting Started* chapter for instructions on how to
install, build and use the TF-A Tests.

See :ref:`Framework Design` for information on how the TF-A Tests work
internally.

See content under the *Porting* chapter for information about how to use this
software on another Armv8-A platform.

See content under the *Process* chapter for information on how to contribute to
this project.

--------------

*Copyright (c)2019, Arm Limited. All rights reserved.*

.. _Juno Arm Development Platform: https://developer.arm.com/products/system-design/development-boards/juno-development-board
.. _trustedfirmware.org: https://git.trustedfirmware.org/TF-A/tf-a-tests.git
.. _Trusted Firmware-A (TF-A): https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git
