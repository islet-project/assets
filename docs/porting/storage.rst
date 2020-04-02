Storage abstraction layer
=========================

In order to improve platform independence and portability a storage abstraction
layer is used to store test results to non-volatile platform storage.

Each platform should register devices and their drivers via the Storage layer.
These drivers then need to be initialized in ``tftf_platform_setup()`` function.

It is mandatory to implement at least one storage driver. For the FVP and Juno
platforms the NOR Flash driver is provided as the default means to store test
results to storage. The storage layer is described in the header file
``include/lib/io_storage.h``. The implementation of the common library is in
``drivers/io/io_storage.c`` and the driver files are located in ``drivers/io/``.

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*
