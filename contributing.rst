Contributing to Trusted Firmware-A Tests
========================================

Getting Started
---------------

-  Make sure you have a Github account and you are logged on
   `developer.trustedfirmware.org`_.

   For any non-trivial piece of work, we advise you to create a `task`_ if one
   does not already exist. This gives everyone visibility of whether others are
   working on something similar.

   Please select the `Trusted Firmware-A Tests` tag in the task creation form.

-  Clone the repository from the Gerrit server. The project details may be found
   on the `tf-a-tests project page`_. We recommend the "`Clone with commit-msg
   hook`" clone method, which will setup the git commit hook that automatically
   generates and inserts appropriate `Change-Id:` lines in your commit messages.

-  Base you work on the latest ``master`` branch.

Making Changes
--------------

-  Make commits of logical units. See these general `Git guidelines`_ for
   contributing to a project.

-  Follow the `Linux coding style`_; this style is enforced for the TF-A Tests
   project (style errors only, not warnings).

   Use the ``checkpatch.pl`` script provided with the Linux source tree. A
   Makefile target is provided for convenience (see section `Checking source
   code style`_ in the User Guide).

-  Keep the commits on topic. If you need to fix another bug or make another
   enhancement, please address it on a separate patch series.

-  Avoid long commit series. If you do have a long series, consider whether
   some commits should be squashed together or addressed in a separate series.

-  Where appropriate, please update the documentation.

-  Ensure that each changed file has the correct copyright and license
   information.

-  Please test your changes. As a minimum, ensure that all standard tests pass
   on the Foundation FVP. See `Running the TF-A Tests`_ for more information.

Submitting Changes
------------------

-  Ensure that each commit in the series has at least one ``Signed-off-by:``
   line, using your real name and email address. If anyone else contributes to
   the commit, they must also add their own ``Signed-off-by:`` line.

   By adding this line the contributor certifies the contribution is made under
   the terms of the `Developer Certificate of Origin (DCO)`_.

   More details may be found in the `Gerrit Signed-off-by Lines guidelines`_.

-  Ensure that each commit also has a unique ``Change-Id:`` line. If you have
   cloned the repository with the "`Clone with commit-msg hook`" clone method
   (as advised in section `Getting started`_), this should already be the case.

   More details may be found in the `Gerrit Change-Ids documentation`_.

-  Submit your patches to the Gerrit review server. Please choose a Gerrit topic
   for your patch series - that is, a short tag associated with all of the
   changes in the same group, such as the local topic branch name.

   Refer to the `Gerrit documentation`_ for more details.

-  Once the patch has had sufficient peer review and has passed the checks run
   by the continuous integration system, the `maintainers`_ will merge your
   patch.

--------------

*Copyright (c) 2018, Arm Limited. All rights reserved.*

.. _maintainers: maintainers.rst
.. _license.rst: license.rst
.. _Developer Certificate of Origin (DCO): dco.txt
.. _Checking source code style: docs/user-guide.rst#checking-source-code-style
.. _Running the TF-A Tests: docs/user-guide.rst#running-the-tf-a-tests

.. _Git guidelines: http://git-scm.com/book/ch5-2.html
.. _Linux coding style: https://www.kernel.org/doc/html/latest/process/coding-style.html

.. _developer.trustedfirmware.org: https://developer.trustedfirmware.org
.. _task: https://developer.trustedfirmware.org/maniphest/query/open/
.. _tf-a-tests project page: https://review.trustedfirmware.org/#/admin/projects/TF-A/tf-a-tests
.. _Gerrit documentation: https://review.trustedfirmware.org/Documentation/user-upload.html
.. _Gerrit Signed-off-by Lines guidelines: https://review.trustedfirmware.org/Documentation/user-signedoffby.html
.. _Gerrit Change-Ids documentation: https://review.trustedfirmware.org/Documentation/user-changeid.html
