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

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*

.. _Linux master tree: https://github.com/torvalds/linux/tree/master/
