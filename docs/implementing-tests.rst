How to implement tests
======================

.. section-numbering::
    :suffix: .

.. contents::

This document aims at providing some pointers to help implementing new tests in
the TFTF image.

Structure of a test
-------------------

A test might be divided into 3 logical parts, detailed in the following
sections.

Prologue
''''''''

A test has a main entry point function, whose type is:

::

    typedef test_result_t (*test_function_t)(void);

See `tftf/framework/include/tftf.h`_.

Only the primary CPU enters this function, while other CPUs are powered down.

First of all, the test function should check whether this test is applicable to
this platform and environment. Some tests rely on specific hardware features or
firmware capabilities to be present. If these are not available, the test should
be skipped.  For example, a multi-core test requires at least 2 CPUs to
run. Macros and functions are provided in `include/common/test_helpers.h`_ to
help test code verify that their requirements are met.

Core
''''

This is completely dependent on the purpose of the test. The paragraphs below
just provide some useful, general information.

The primary CPU may power on other CPUs by calling the function
``tftf_cpu_on()``.  It provides an address to which secondary CPUs should jump
to once they have been initialized by the test framework. This address should be
different from the primary CPU test function.

Synchronization primitives are provided in `include/lib/events.h`_ in case CPUs'
execution threads need to be synchronized. Most multi-processing tests will need
some synchronisation points that all/some CPUs need to reach before test
execution may continue.

Any CPU that is involved in a test must return from its test function. Failure
to do so will put the framework in an unrecoverable state, see the `TFTF known
limitations`_. The return code indicates the test result from the point of view
of this CPU. At the end of the test, individual CPU results are aggregated and
the overall test result is derived from that. A test is considered as passed if
all involved CPUs reported a success status code.

Epilogue
''''''''

Each test is responsible for releasing any allocated resources and putting the
system back in a clean state when it finishes. Any change to the system
configuration (e.g. MMU setup, GIC configuration, system registers, ...) must be
undone and the original configuration must be restored. This guarantees that the
next test is not affected by the actions of the previous one.

One exception to this rule is that CPUs powered on as part of a test must not be
powered down. As already stated above, as soon as a CPU enters the test, the
framework expects it to return from the test.

Template test code
------------------

Some template test code is provided in `tftf/tests/template_tests`_. It can be
used as a starting point for developing new tests. Template code for both
single-core and multi-core tests is provided.

Plugging the test into the build system
---------------------------------------

All test code is located under the `tftf/tests`_ directory. Tests are usually
divided into categories represented as sub-directories under ``tftf/tests/``.

The source file implementing the new test code should be added to the
appropriate tests makefile, see `.*mk` files under `tftf/tests`_.

The new test code should also appear in a tests manifest, see ``*.xml`` files
under `tftf/tests`_. A unique name and test function must be provided. An
optional description may be provided as well.

For example, to create a test case named "``Foo test case``", whose test
function is ``foo()``, add the following line in the tests manifest:

::

    <testcase name="Foo test case" function="foo" />

A testcase must be part of a testsuite. The ``testcase`` XML node above must be
inside a ``testsuite`` XML node. A unique name and a description must be
provided for the testsuite.

For example, to create a test suite named "``Bar test suite``", whose
description is: "``An example test suite``", add the following 2 lines:

::

    <testsuite name="Bar test suite" description="An example test suite">
    </testsuite>

See the template test manifest for reference: `tftf/tests/tests-template.xml`_.

--------------

*Copyright (c) 2018, Arm Limited. All rights reserved.*

.. _SMC Calling Convention: SMCCC_
.. _SMCCC: http://infocenter.arm.com/help/topic/com.arm.doc.den0028b/ARM_DEN0028B_SMC_Calling_Convention.pdf

.. _TFTF known limitations: change-log.rst#test-framework
.. _tftf/framework/include/tftf.h: ../tftf/framework/include/tftf.h
.. _tftf/tests: ../tftf/tests
.. _tftf/tests/template_tests: ../tftf/tests/template_tests
.. _tftf/tests/tests-template.xml: ../tftf/tests/tests-template.xml
.. _include/common/test_helpers.h: ../include/common/test_helpers.h
.. _include/lib/events.h: ../include/lib/events.h
