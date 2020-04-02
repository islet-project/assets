Build Flags
===========

The platform may define build flagsto to control inclusion or exclusion of
certain tests. These flags must be defined in the platform makefile which
is included by the build system.

-  **PLAT_TESTS_SKIP_LIST**

This build flag can be defined by the platform to control exclusion of some
testcases from the default test plan for a platform. If used this needs to
point to a text file which follows the following criteria:

  -  Contain a list of tests to skip for this platform.

  -  Specify 1 test per line, using the following format:

     ::

       testsuite_name/testcase_name

     where ``testsuite_name`` and ``testcase_name`` are the names that appear in
     the XML tests file.

  -  Alternatively, it is possible to disable a test suite entirely, which will
     disable all test cases part of this test suite. To do so, only specify the
     test suite name, omitting the ``/testcase_name`` part.

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*
