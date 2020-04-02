Mandatory Modifications
=======================

File : platform_def.h
---------------------

Each platform must ensure that a header file of this name is in the system
include path with the following constants defined. This may require updating the
list of ``PLAT_INCLUDES`` in the ``platform.mk`` file. In the ARM FVP port, this
file is found in ``plat/arm/board/fvp/include/platform_def.h``.

-  **#define : PLATFORM_LINKER_FORMAT**

   Defines the linker format used by the platform, for example
   `elf64-littleaarch64` used by the FVP.

-  **#define : PLATFORM_LINKER_ARCH**

   Defines the processor architecture for the linker by the platform, for
   example `aarch64` used by the FVP.

-  **#define : PLATFORM_STACK_SIZE**

   Defines the stack memory available to each CPU. This constant is used by
   ``plat/common/aarch64/platform_mp_stack.S``.

-  **#define : PLATFORM_CLUSTER_COUNT**

   Defines the total number of clusters implemented by the platform in the
   system.

-  **#define : PLATFORM_CORE_COUNT**

   Defines the total number of CPUs implemented by the platform across all
   clusters in the system.

-  **#define : PLATFORM_NUM_AFFS**

   Defines the total number of nodes in the affinity hierarchy at all affinity
   levels used by the platform.

-  **#define : PLATFORM_MAX_AFFLVL**

   Defines the maximum number of affinity levels in the system that the platform
   implements.  ARMv8-A has support for 4 affinity levels. It is likely that
   hardware will implement fewer affinity levels. For example, the Base AEM FVP
   implements two clusters with a configurable number of CPUs.  It reports the
   maximum affinity level as 1.

-  **#define : PLAT_MAX_SPI_OFFSET_ID**

   Defines the offset of the last Shared Peripheral Interrupt supported by the
   TF-A Tests on this platform. SPI numbers are mapped onto GIC interrupt IDs,
   starting from interrupt ID 32. In other words, this offset ID corresponds to
   the last SPI number, to which 32 must be added to get the corresponding last
   GIC IRQ ID.

   E.g. If ``PLAT_MAX_SPI_OFFSET_ID`` is 10, this means that IRQ #42 is the last
   SPI.

-  **#define : PLAT_LOCAL_PSTATE_WIDTH**

   Defines the bit-field width of the local state in State-ID field of the
   power-state parameter. This macro will be used to compose the State-ID field
   given the local power state at different affinity levels.

-  **#define : PLAT_MAX_PWR_STATES_PER_LVL**

   Defines the maximum number of power states at a power domain level for the
   platform. This macro will be used by the ``PSCI_STAT_COUNT/RESIDENCY`` tests
   to determine the size of the array to allocate for storing the statistics.

-  **#define : TFTF_BASE**

   Defines the base address of the TFTF binary in DRAM. Used by the linker
   script to link the image at the right address. Must be aligned on a page-size
   boundary.

-  **#define : IRQ_PCPU_NS_TIMER**

   Defines the IRQ ID of the per-CPU Non-Secure timer of the platform.

-  **#define : IRQ_CNTPSIRQ1**

   Defines the IRQ ID of the System timer of the platform.

-  **#define : TFTF_NVM_OFFSET**

   The TFTF needs some Non-Volatile Memory to store persistent data. This
   defines the offset from the beginning of this memory that the TFTF can use.

-  **#define : TFTF_NVM_SIZE**

   Defines the size of the Non-Volatile Memory allocated for TFTF usage.

If the platform port uses the ARM Watchdog Module (`SP805`_) peripheral, the
following constant needs to be defined:

-  **#define : SP805_WDOG_BASE**

   Defines the base address of the `SP805`_ watchdog peripheral.

If the platform port uses the IO storage framework, the following constants
must also be defined:

-  **#define : MAX_IO_DEVICES**

   Defines the maximum number of registered IO devices. Attempting to register
   more devices than this value using ``io_register_device()`` will fail with
   ``IO_RESOURCES_EXHAUSTED``.

-  **#define : MAX_IO_HANDLES**

   Defines the maximum number of open IO handles. Attempting to open more IO
   entities than this value using ``io_open()`` will fail with
   ``IO_RESOURCES_EXHAUSTED``.

If the platform port uses the VExpress NOR flash driver (see
``drivers/io/vexpress_nor/``), the following constants must also be defined:

-  **#define : NOR_FLASH_BLOCK_SIZE**

   Defines the largest block size as seen by the software while writing to NOR
   flash.

Function : tftf_plat_arch_setup()
---------------------------------
::

    Argument : void
    Return   : void

This function performs any platform-specific and architectural setup that the
platform requires.

In both the ARM FVP and Juno ports, this function configures and enables the
MMU.

Function : tftf_early_platform_setup()
--------------------------------------

::

    Argument : void
    Return   : void

This function executes with the MMU and data caches disabled. It is only called
by the primary CPU. It is used to perform platform-specific actions very early
in the boot.

In both the ARM FVP and Juno ports, this function configures the console.

Function : tftf_platform_setup()
--------------------------------

::

    Argument : void
    Return   : void

This function executes with the MMU and data caches enabled. It is responsible
for performing any remaining platform-specific setup that can occur after the
MMU and data cache have been enabled.

This function is also responsible for initializing the storage abstraction layer
used to access non-volatile memory for permanent storage of test results. It
also initialises the GIC and detects the platform topology using
platform-specific means.

Function : plat_get_nvm_handle()
--------------------------------

::

    Argument : uintptr_t *
    Return   : void

It is needed if the platform port uses IO storage framework. This function is
responsible for getting the pointer to the initialised non-volatile memory
entity.

Function : tftf_plat_get_pwr_domain_tree_desc()
-----------------------------------------------

::

    Argument : void
    Return   : const unsigned char *

This function returns the platform topology description array in a suitable
format as expected by TFTF. The size of the array is expected to be
``PLATFORM_NUM_AFFS - PLATFORM_CORE_COUNT + 1``. The format used to describe
this array is :

1.  The first entry in the array specifies the number of power domains at the
    highest power level implemented in the platform. This caters for platforms
    where the power domain tree does not have a single root node e.g. the FVP
    which has two cluster power domains at the highest level (that is, 1).

2.  Each subsequent entry corresponds to a power domain and contains the number
    of power domains that are its direct children.

The array format is the same as the one used by Trusted Firmware-A and more
details of its description can be found in the Trusted Firmware-A documentation:
`docs/psci-pd-tree.rst`_.

Function : tftf_plat_get_mpidr()
--------------------------------

::

    Argument : unsigned int
    Return   : uint64_t

This function converts a given `core_pos` into a valid MPIDR if the CPU is
present in the platform. The `core_pos` is a unique number less than the
``PLATFORM_CORE_COUNT`` returned by ``platform_get_core_pos()`` for a given
CPU. This API is used by the topology framework in TFTF to query the presence of
a CPU and, if present, returns the corresponding MPIDR for it. If the CPU
referred to by the `core_pos` is absent, then this function returns
``INVALID_MPID``.

Function : plat_get_state_prop()
--------------------------------

::

    Argument : unsigned int
    Return   : const plat_state_prop_t *

This functions returns the ``plat_state_prop_t`` array for all the valid low
power states from platform for a specified affinity level and returns ``NULL``
for an invalid affinity level. The array is expected to be NULL-terminated.
This function is expected to be used by tests that need to compose the power
state parameter for use in ``PSCI_CPU_SUSPEND`` API or ``PSCI_STAT/RESIDENCY``
API.

Function : plat_fwu_io_setup()
------------------------------

::

    Argument : void
    Return   : void

This function initializes the IO system used by the firmware update.

Function : plat_arm_gic_init()
------------------------------

::

    Argument : void
    Return   : void

This function initializes the ARM Generic Interrupt Controller (GIC).

Function : platform_get_core_pos()
----------------------------------

::

    Argument : u_register_t
    Return   : unsigned int

This function returns a linear core ID from a MPID.

Function : plat_crash_console_init()
------------------------------------

::

    Argument : void
    Return   : int

This function initializes a platform-specific console for crash reporting.

Function : plat_crash_console_putc()
------------------------------------

::

    Argument : int
    Return   : int

This function prints a character on the platform-specific crash console.

Function : plat_crash_console_flush()
-------------------------------------

::

    Argument : void
    Return   : int

This function waits until all the characters of the platform-specific crash
console have been actually printed.

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*

.. _SP805: https://static.docs.arm.com/ddi0270/b/DDI0270.pdf
.. _docs/psci-pd-tree.rst: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/psci-pd-tree.rst
