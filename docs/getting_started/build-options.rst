Build Options Summary
=====================

As far as possible, TF-A Tests dynamically detects the platform hardware
components and available features. There are a few build options to select
specific features where the dynamic detection falls short.

Unless mentioned otherwise, these options are expected to be specified at the
build command line and are not to be modified in any component makefiles.

.. note::
   The build system doesn't track dependencies for build options. Therefore, if
   any of the build options are changed from a previous build, a clean build
   must be performed.

Common (Shared) Build Options
-----------------------------

Most of the build options listed in this section apply to TFTF, the FWU test
images and Cactus, unless otherwise specified. These do not influence the EL3
payload, whose simplistic build system is mostly independent.

-  ``ARCH``: Choose the target build architecture for TF-A Tests. It can take
   either ``aarch64`` or ``aarch32`` as values. By default, it is defined to
   ``aarch64``. Not all test images support this build option.

-  ``ARM_ARCH_FEATURE``: Optional Arm Architecture build option which specifies
   one or more feature modifiers. This option has the form ``[no]feature+...``
   and defaults to ``none``. It translates into compiler option
   ``-march=armvX[.Y]-a+[no]feature+...``. See compiler's documentation for the
   list of supported feature modifiers.

-  ``ARM_ARCH_MAJOR``: The major version of Arm Architecture to target when
   compiling TF-A Tests. Its value must be numeric, and defaults to 8.

-  ``ARM_ARCH_MINOR``: The minor version of Arm Architecture to target when
   compiling TF-A Tests. Its value must be a numeric, and defaults to 0.

-  ``BRANCH_PROTECTION``: Numeric value to enable ARMv8.3 Pointer Authentication
   (``ARMv8.3-PAuth``) and ARMv8.5 Branch Target Identification (``ARMv8.5-BTI``)
   support in the Trusted Firmware-A Test Framework itself.
   If enabled, it is needed to use a compiler that supports the option
   ``-mbranch-protection`` (GCC 9 and later).
   Selects the branch protection features to use:
-  0: Default value turns off all types of branch protection
-  1: Enables all types of branch protection features
-  2: Return address signing to its standard level
-  3: Extend the signing to include leaf functions
-  4: Turn on branch target identification mechanism

   The table below summarizes ``BRANCH_PROTECTION`` values, GCC compilation
   options and resulting PAuth/BTI features.

   +-------+--------------+-------+-----+
   | Value |  GCC option  | PAuth | BTI |
   +=======+==============+=======+=====+
   |   0   |     none     |   N   |  N  |
   +-------+--------------+-------+-----+
   |   1   |   standard   |   Y   |  Y  |
   +-------+--------------+-------+-----+
   |   2   |   pac-ret    |   Y   |  N  |
   +-------+--------------+-------+-----+
   |   3   | pac-ret+leaf |   Y   |  N  |
   +-------+--------------+-------+-----+
   |   4   |     bti      |   N   |  Y  |
   +-------+--------------+-------+-----+

   This option defaults to 0 and this is an experimental feature.

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

Arm FVP Platform Specific Build Options
---------------------------------------

-  ``FVP_CLUSTER_COUNT`` : Configures the cluster count to be used to build the
   topology tree within TFTF. By default TFTF is configured for dual cluster for
   CPUs with single thread (ST) and single cluster for SMT CPUs.
   For ST CPUs this option can be used to override the default number of clusters
   with a value in the range 1-4.

-  ``FVP_MAX_CPUS_PER_CLUSTER``: Sets the maximum number of CPUs implemented in
   a single cluster. This option defaults to the maximum value of 4 for ST CPUs
   and maximum value of 8 for SMT CPUs.

-  ``FVP_MAX_PE_PER_CPU``: Sets the maximum number of PEs implemented on any CPU
   in the system. This option defaults to 1 to select ST CPUs. For platforms with
   SMT CPUs this value must be set to 2.

TFTF-specific Build Options
---------------------------

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

Realm payload specific Build Options
------------------------------------

-  ``TFTF_MAX_IMAGE_SIZE``: The option needs to be either set by the user or
   by the platform makefile to specify the maximum size of TFTF binary. This
   is needed so that the Realm payload binary can be appended to TFTF binary
   via ``make pack_realm`` build command.

FWU-specific Build Options
--------------------------

-  ``FIRMWARE_UPDATE``: Whether the Firmware Update test images (i.e.
   ``NS_BL1U`` and ``NS_BL2U``) should be built. The default value is 0.  The
   platform makefile is free to override this value if Firmware Update is
   supported on this platform.

--------------

*Copyright (c) 2019-2020, Arm Limited. All rights reserved.*
