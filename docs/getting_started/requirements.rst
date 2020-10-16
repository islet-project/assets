Prerequisites & Requirements
============================

This document describes the software and hardware requiremnts for building TF-A
Tests for AArch32 and AArch64 target platforms.

It may be possible to build TF-A Tests with combinations of software and
hardware that are different from those listed below. The software and hardware
described in this document are officially supported.

Build Host
----------

TF-A Tests may be built using a Linux build host machine with a recent Linux
distribution. We have performed tests using Ubuntu 16.04 LTS (64-bit), but other
distributions should also work fine, provided that the tools and libraries
can be installed.

Toolchain
---------

Install the required packages to build TF-A Tests with the following command:

::

    sudo apt-get install device-tree-compiler build-essential git perl libxml-libxml-perl

Download and install the GNU cross-toolchain from Linaro. The TF-A Tests have
been tested with version 9.2-2019.12 (gcc 9.2):

-  `GCC cross-toolchain`_

In addition, the following optional packages and tools may be needed:

-   For debugging, Arm `Development Studio 5 (DS-5)`_.

.. _GCC cross-toolchain: https://developer.arm.com/open-source/gnu-toolchain/gnu-a/downloads
.. _Development Studio 5 (DS-5): https://developer.arm.com/products/software-development-tools/ds-5-development-studio

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*
