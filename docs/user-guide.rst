Trusted Firmware-A Tests - User Guide
=====================================

.. section-numbering::
    :suffix: .

.. contents::

This document describes how to build the Trusted Firmware-A Tests (TF-A Tests)
and run them on a set of platforms. It assumes that the reader has previous
experience building and running the `Trusted Firmware-A (TF-A)`_.

Host machine requirements
-------------------------

The minimum recommended machine specification for building the software and
running the `FVP models`_ is a dual-core processor running at 2GHz with 12GB of
RAM. For best performance, use a machine with a quad-core processor running at
2.6GHz with 16GB of RAM.

The software has been tested on Ubuntu 16.04 LTS (64-bit). Packages used for
building the software were installed from that distribution unless otherwise
specified.

Tools
-----

Install the required packages to build TF-A Tests with the following command:

::

    sudo apt-get install device-tree-compiler build-essential make git perl libxml-libxml-perl

Download and install the GNU cross-toolchain from Linaro. The TF-A Tests have
been tested with version 6.2-2016.11 (gcc 6.2):

-  `AArch32 GNU cross-toolchain`_
-  `AArch64 GNU cross-toolchain`_

In addition, the following optional packages and tools may be needed:

-   For debugging, Arm `Development Studio 5 (DS-5)`_.

Getting the TF-A Tests source code
----------------------------------

Download the TF-A Tests source code using the following command:

::

    git clone https://git.trustedfirmware.org/TF-A/tf-a-tests.git

Building TF-A Tests
-------------------

-  Before building TF-A Tests, the environment variable ``CROSS_COMPILE`` must
   point to the Linaro cross compiler.

   For AArch64:

   ::

       export CROSS_COMPILE=<path-to-aarch64-gcc>/bin/aarch64-linux-gnu-

   For AArch32:

   ::

       export CROSS_COMPILE=<path-to-aarch32-gcc>/bin/arm-linux-gnueabihf-

-  Change to the root directory of the TF-A Tests source tree and build.

   For AArch64:

   ::

       make PLAT=<platform>

   For AArch32:

   ::

       make PLAT=<platform> ARCH=aarch32

   Notes:

   -  If ``PLAT`` is not specified, ``fvp`` is assumed by default. See the
      `Summary of build options`_ for more information on available build
      options.

   -  By default this produces a release version of the build. To produce a
      debug version instead, build the code with ``DEBUG=1``.

   -  The build process creates products in a ``build/`` directory tree,
      building the objects and binaries for each test image in separate
      sub-directories. The following binary files are created from the
      corresponding ELF files:

      -  ``build/<platform>/<build-type>/tftf.bin``
      -  ``build/<platform>/<build-type>/ns_bl1u.bin``
      -  ``build/<platform>/<build-type>/ns_bl2u.bin``
      -  ``build/<platform>/<build-type>/el3_payload.bin``
      -  ``build/<platform>/<build-type>/cactus.bin``

      where ``<platform>`` is the name of the chosen platform and ``<build-type>``
      is either ``debug`` or ``release``. The actual number of images might differ
      depending on the platform.

      Refer to the sections below for more information about each image.

-  Build products for a specific build variant can be removed using:

   ::

       make DEBUG=<D> PLAT=<platform> clean

   ... where ``<D>`` is ``0`` or ``1``, as specified when building.

   The build tree can be removed completely using:

   ::

       make realclean

-  Use the following command to list all supported build commands:

   ::

       make help

TFTF test image
```````````````

``tftf.bin`` is the main test image to exercise the TF-A features. The other
test images provided in this repository are optional dependencies that TFTF
needs to test some specific features.

``tftf.bin`` may be built independently of the other test images using the
following command:

::

   make PLAT=<platform> tftf

In TF-A boot flow, ``tftf.bin`` replaces the ``BL33`` image and should be
injected in the FIP image. This might be achieved by running the following
command from the TF-A root directory:

::

    BL33=tftf.bin make PLAT=<platform> fip

Please refer to the `TF-A User guide`_ for further details.

NS_BL1U and NS_BL2U test images
```````````````````````````````

``ns_bl1u.bin`` and ``ns_bl2u.bin`` are test images that exercise the `Firmware
Update`_ (FWU) feature of TF-A [#]_. Throughout this document, they will be
referred as the `FWU test images`.

In addition to updating the firmware, the FWU test images also embed some tests
that exercise the `FWU state machine`_ implemented in the TF-A. They send valid
and invalid SMC requests to the TF-A BL1 image in order to test its robustness.

NS_BL1U test image
''''''''''''''''''

The ``NS_BL1U`` image acts as the `Application Processor (AP) Firmware Update
Boot ROM`. This typically is the first software agent executing on the AP in the
Normal World during a firmware update operation. Its primary purpose is to load
subsequent firmware update images from an external interface, such as NOR Flash,
and communicate with ``BL1`` to authenticate those images.

The ``NS_BL1U`` test image provided in this repository performs the following
tasks:

-  Load FWU images from external non-volatile storage (typically flash memory)
   to Non-Secure RAM.

-  Request TF-A BL1 to copy these images in Secure RAM and authenticate them.

-  Jump to ``NS_BL2U`` which carries out the next steps in the firmware update
   process.

This image may be built independently of the other test images using the
following command:

::

   make PLAT=<platform> ns_bl1u

NS_BL2U test image
''''''''''''''''''

The ``NS_BL2U`` image acts as the `AP Firmware Updater`. Its primary
responsibility is to load a new set of firmware images from an external
interface and write them into non-volatile storage.

The ``NS_BL2U`` test image provided in this repository overrides the original
FIP image stored in flash with the backup FIP image (see below).

This image may be built independently of the other test images using the
following command:

::

   make PLAT=<platform> ns_bl2u

Putting it all together
'''''''''''''''''''''''

The FWU test images should be used in conjunction with the TFTF image, as the
latter initiates the FWU process by corrupting the FIP image and resetting the
target. Once the FWU process is complete, TFTF takes over again and checks that
the firmware was successfully updated.

To sum up, 3 images must be built out of the TF-A Tests repository in order to
test the TF-A Firmware Update feature:

-  ``ns_bl1u.bin``
-  ``ns_bl2u.bin``
-  ``tftf.bin``

Once that's done, they must be combined in the right way.

-  ``ns_bl1u.bin`` is a standalone image and does not require any further
   processing.

-  ``ns_bl2u.bin`` must be injected into the ``FWU_FIP`` image. This might be
   achieved by setting ``NS_BL2U=ns_bl2u.bin`` when building the ``FWU_FIP``
   image out of the TF-A repository. Please refer to the section `Building FIP
   images with support for Trusted Board Boot`_ in the TF-A User Guide.

-  ``tftf.bin`` must be injected in the standard FIP image, as explained
   in section `TFTF test image`_.

Additionally, on Juno platform, the FWU FIP must contain a ``SCP_BL2U`` image.
This image can simply be a copy of the standard ``SCP_BL2`` image if no specific
firmware update operations need to be carried on the SCP side.

Finally, the backup FIP image must be created. This can simply be a copy of the
standard FIP image, which means that the Firmware Update process will restore
the original, uncorrupted FIP image.

EL3 test payload
````````````````

``el3_payload.bin`` is a test image exercising the alternative `EL3 payload boot
flow`_ in TF-A. Refer to the `EL3 test payload README file`_ for more details
about its behaviour and how to build and run it.

Cactus test image
`````````````````

``cactus.bin`` is a test secure partition that exercises the `Secure Partition
Manager`_ (SPM) in TF-A [#]_. It runs in Secure-EL0. It performs the following
tasks:

-  Test that TF-A has correctly setup the secure partition environment: Cactus
   should be allowed to perform cache maintenance operations, access floating
   point registers, etc.

-  Test that TF-A accepts to change data access permissions and instruction
   permissions on behalf of Cactus for memory regions the latter owns.

-  Test communication with SPM through the `ARM Management Mode Interface`_.

At the moment, Cactus is supported on AArch64 FVP only. It may be built
independently of the other test images using the following command:

::

   make PLAT=fvp cactus

In TF-A boot flow, Cactus replaces the ``BL32`` image and should be injected in
the FIP image.  This might be achieved by running the following command from
the TF-A root directory:

::

    BL32=cactus.bin make PLAT=fvp ENABLE_SPM=1 fip

Please refer to the `TF-A User guide`_ for further details.

To run the full set of tests in Cactus, it should be used in conjunction with
the TFTF image, as the latter sends the Management Mode requests that Cactus
services. The TFTF has to be built with `TESTS=spm` to run the SPM tests.

Summary of build options
````````````````````````

As much as possible, TF-A Tests dynamically detect the platform hardware
components and available features. There are a few build options to select
specific features where the dynamic detection falls short. This section lists
them.

Unless mentioned otherwise, these options are expected to be specified at the
build command line and are not to be modified in any component makefiles.

Note that the build system doesn't track dependencies for build options.
Therefore, if any of the build options are changed from a previous build, a
clean build must be performed.

Build options shared across test images
'''''''''''''''''''''''''''''''''''''''

Most of the build options listed in this section apply to TFTF, the FWU test
images and Cactus, unless otherwise specified. These do not influence the EL3
payload, whose simplistic build system is mostly independent.

-  ``ARCH``: Choose the target build architecture for TF-A Tests. It can take
   either ``aarch64`` or ``aarch32`` as values. By default, it is defined to
   ``aarch64``. Not all test images support this build option.

-  ``ARM_ARCH_MAJOR``: The major version of Arm Architecture to target when
   compiling TF-A Tests. Its value must be numeric, and defaults to 8.

-  ``ARM_ARCH_MINOR``: The minor version of Arm Architecture to target when
   compiling TF-A Tests. Its value must be a numeric, and defaults to 0.

-  ``DEBUG``: Chooses between a debug and a release build. A debug build
   typically embeds assertions checking the validity of some assumptions and its
   output is more verbose. The option can take either 0 (release) or 1 (debug)
   as values. 0 is the default.

-  ``ENABLE_ASSERTIONS``: This option controls whether calls to ``assert()`` are
   compiled out.

   -  For debug builds, this option defaults to 1, and calls to ``assert()`` are
      compiled in.
   -  For release builds, this option defaults to 0 and calls to ``assert()``
      are compiled out.

   This option can be set independently of ``DEBUG``. It can also be used to
   hide any auxiliary code that is only required for the assertion and does not
   fit in the assertion itself.

-  ``LOG_LEVEL``: Chooses the log level, which controls the amount of console log
   output compiled into the build. This should be one of the following:

   ::

       0  (LOG_LEVEL_NONE)
       10 (LOG_LEVEL_ERROR)
       20 (LOG_LEVEL_NOTICE)
       30 (LOG_LEVEL_WARNING)
       40 (LOG_LEVEL_INFO)
       50 (LOG_LEVEL_VERBOSE)

   All log output up to and including the selected log level is compiled into
   the build. The default value is 40 in debug builds and 20 in release builds.

-  ``PLAT``: Choose a platform to build TF-A Tests for. The chosen platform name
   must be a subdirectory of any depth under ``plat/``, and must contain a
   platform makefile named ``platform.mk``. For example, to build TF-A Tests for
   the Arm Juno board, select ``PLAT=juno``.

-  ``V``: Verbose build. If assigned anything other than 0, the build commands
   are printed. Default is 0.

TFTF build options
''''''''''''''''''

-  ``NEW_TEST_SESSION``: Choose whether a new test session should be started
   every time or whether the framework should determine whether a previous
   session was interrupted and resume it. It can take either 1 (always
   start new session) or 0 (resume session as appropriate). 1 is the default.

-  ``TESTS``: Set of tests to run. Use the following command to list all
   possible sets of tests:

   ::

     make help_tests

   If no set of tests is specified, the standard tests will be selected (see
   ``tftf/tests/tests-standard.xml``).

-  ``USE_NVM``: Used to select the location of test results. It can take either 0
   (RAM) or 1 (non-volatile memory like flash) as test results storage. Default
   value is 0, as writing to the flash significantly slows tests down.

FWU test images build options
'''''''''''''''''''''''''''''

-  ``FIRMWARE_UPDATE``: Whether the Firmware Update test images (i.e.
   ``NS_BL1U`` and ``NS_BL2U``) should be built. The default value is 0.  The
   platform makefile is free to override this value if Firmware Update is
   supported on this platform.

Checking source code style
--------------------------

When making changes to the source for submission to the project, the source must
be in compliance with the Linux style guide. To assist with this, the project
Makefile provides two targets, which both utilise the ``checkpatch.pl`` script
that ships with the Linux source tree.

To check the entire source tree, you must first download copies of
``checkpatch.pl``, ``spelling.txt`` and ``const_structs.checkpatch`` available
in the `Linux master tree`_ scripts directory, then set the ``CHECKPATCH``
environment variable to point to ``checkpatch.pl`` (with the other 2 files in
the same directory).

Then use the following command:

::

    make CHECKPATCH=<path-to-linux>/linux/scripts/checkpatch.pl checkcodebase

To limit the coding style checks to your local changes, use:

::

    make CHECKPATCH=<path-to-linux>/linux/scripts/checkpatch.pl checkpatch

By default, this will check all patches between ``origin/master`` and your local
branch. If you wish to use a different reference commit, this can be specified
using the ``BASE_COMMIT`` variable.

Running the TF-A Tests
----------------------

Refer to the sections `Running the software on FVP`_ and `Running the software
on Juno`_ in `TF-A User Guide`_. The same instructions mostly apply to run the
TF-A Tests on those 2 platforms. The difference is that the following images are
not needed here:

-  Normal World bootloader. The TFTF replaces it in the boot flow;

-  Linux Kernel;

-  Device tree;

-  Filesystem.

In other words, only the following software images are needed:

-  ``BL1`` firmware image;

-  ``FIP`` image containing the following images:

   -  ``BL2``;
   -  ``SCP_BL2`` if required by the platform (e.g. Juno);
   -  ``BL31``;
   -  ``BL32`` (optional);
   -  ``tftf.bin`` (standing as the BL33 image).

Running the FWU tests
`````````````````````

As previously mentioned in section `Putting it all together`_, there are a
couple of extra images involved when running the FWU tests. They need to be
loaded at the right addresses, which depend on the platform.

FVP
'''

In addition to the usual BL1 and FIP images, the following extra images must be
loaded:

-  ``NS_BL1U`` image at address ``0x0BEB8000`` (i.e. NS_BL1U_BASE macro in TF-A)
-  ``FWU_FIP`` image at address ``0x08400000`` (i.e. NS_BL2U_BASE macro in TF-A)
-  ``Backup FIP`` image at address ``0x09000000`` (i.e. FIP_BKP_ADDRESS macro in
   TF-A tests).

An example script is provided in `scripts/run_fwu_fvp.sh`_.

Juno
''''

The same set of extra images and load addresses apply for Juno as for FVP.

The new images must be programmed in flash memory by adding some entries in the
``SITE1/HBI0262x/images.txt`` configuration file on the Juno SD card (where
``x`` depends on the revision of the Juno board). Refer to the `Juno Getting
Started Guide`_, section 2.3 "Flash memory programming" for more
information. Users should ensure these do not overlap with any other entries in
the file.

Addresses in this file are expressed as an offset from the base address of the
flash (that is, ``0x08000000``).

::

    NOR10UPDATE: AUTO                       ; Image Update:NONE/AUTO/FORCE
    NOR10ADDRESS: 0x00400000                ; Image Flash Address
    NOR10FILE: \SOFTWARE\fwu_fip.bin        ; Image File Name
    NOR10LOAD: 00000000                     ; Image Load Address
    NOR10ENTRY: 00000000                    ; Image Entry Point

    NOR11UPDATE: AUTO                       ; Image Update:NONE/AUTO/FORCE
    NOR11ADDRESS: 0x03EB8000                ; Image Flash Address
    NOR11FILE: \SOFTWARE\ns_bl1u.bin        ; Image File Name
    NOR11LOAD: 00000000                     ; Image Load Address
    NOR11ENTRY: 00000000                    ; Image Load Address

    NOR12UPDATE: AUTO                       ; Image Update:NONE/AUTO/FORCE
    NOR12ADDRESS: 0x01000000                ; Image Flash Address
    NOR12FILE: \SOFTWARE\backup_fip.bin     ; Image File Name
    NOR12LOAD: 00000000                     ; Image Load Address
    NOR12ENTRY: 00000000                    ; Image Entry Point

--------------

.. [#] Therefore, the Trusted Board Boot feature must be enabled in TF-A for
       the FWU test images to work. Please refer the `TF-A User guide`_ for
       further details.

.. [#] Therefore, the Secure Partition Manager must be enabled in TF-A for
       Cactus to work. Please refer to the `TF-A User guide`_ for further
       details.

--------------

*Copyright (c) 2018, Arm Limited. All rights reserved.*

.. _Development Studio 5 (DS-5): https://developer.arm.com/products/software-development-tools/ds-5-development-studio

.. _FVP models: https://developer.arm.com/products/system-design/fixed-virtual-platforms

.. _AArch32 GNU cross-toolchain: http://releases.linaro.org/components/toolchain/binaries/6.2-2016.11/arm-linux-gnueabihf/gcc-linaro-6.2.1-2016.11-x86_64_arm-linux-gnueabihf.tar.xz
.. _AArch64 GNU cross-toolchain: http://releases.linaro.org/components/toolchain/binaries/6.2-2016.11/aarch64-linux-gnu/gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu.tar.xz

.. _Linux master tree: https://github.com/torvalds/linux/tree/master/

.. _TF-A: https://www.github.com/ARM-software/arm-trusted-firmware
.. _Trusted Firmware-A (TF-A): TF-A_
.. _EL3 payload boot flow: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/user-guide.rst#el3-payloads-alternative-boot-flow
.. _TF-A User Guide: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/user-guide.rst
.. _Firmware Update: FWU_
.. _FWU: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/firmware-update.rst
.. _FWU state machine: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/firmware-update.rst#fwu-state-machine
.. _Running the software on FVP: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/user-guide.rst#running-the-software-on-fvp
.. _Running the software on Juno: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/user-guide.rst#running-the-software-on-juno
.. _Building FIP images with support for Trusted Board Boot: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/user-guide.rst#building-fip-images-with-support-for-trusted-board-boot
.. _Secure partition Manager: https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/secure-partition-manager-design.rst

.. _EL3 test payload README file: ../el3_payload/README
.. _scripts/run_fwu_fvp.sh: ../scripts/run_fwu_fvp.sh

.. _ARM Management Mode Interface: http://infocenter.arm.com/help/topic/com.arm.doc.den0060a/DEN0060A_ARM_MM_Interface_Specification.pdf
.. _Juno Getting Started Guide: http://infocenter.arm.com/help/topic/com.arm.doc.dui0928e/DUI0928E_juno_arm_development_platform_gsg.pdf
