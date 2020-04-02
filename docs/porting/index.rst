Porting
=======

.. toctree::
   :maxdepth: 1

   requirements
   storage
   build-flags
   mandatory-mods
   optional-mods

Porting the TF-A Tests to a new platform involves making some mandatory and
optional modifications for both the cold and warm boot paths. Modifications
consist of:

*   Implementing a platform-specific function or variable,
*   Setting up the execution context in a certain way, or
*   Defining certain constants (for example #defines).

The platform-specific functions and variables are all declared in
``include/plat/common/platform.h``. The framework provides a default
implementation of variables and functions to fulfill the optional requirements.
These implementations are all weakly defined; they are provided to ease the
porting effort. Each platform port can override them with its own implementation
if the default implementation is inadequate.

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*
