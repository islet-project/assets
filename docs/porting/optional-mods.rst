Optional Modifications
======================

The following are helper functions implemented by the test framework that
perform common platform-specific tasks. A platform may choose to override these
definitions.

Function : platform_get_stack()
-------------------------------

::

    Argument : unsigned long
    Return   : unsigned long

This function returns the base address of the memory stack that has been
allocated for the CPU specified by MPIDR. The size of the stack allocated to
each CPU is specified by the platform defined constant ``PLATFORM_STACK_SIZE``.

Common implementation of this function is provided in
``plat/common/aarch64/platform_mp_stack.S``.

Function : tftf_platform_end()
------------------------------

::

    Argument : void
    Return   : void

This function performs any operation required by the platform to properly finish
the test session.

The default implementation sends an EOT (End Of Transmission) character on the
UART. This can be used to automatically shutdown the FVP models. When running on
real hardware, the UART output may be parsed by an external tool looking for
this character and rebooting the platform for example.

Function : tftf_plat_reset()
----------------------------

::

    Argument : void
    Return   : void

This function resets the platform.

The default implementation uses the ARM watchdog peripheral (`SP805`_) to
generate a watchdog timeout interrupt. This interrupt remains deliberately
unserviced, which eventually asserts the reset signal.

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*

.. _SP805: https://static.docs.arm.com/ddi0270/b/DDI0270.pdf
