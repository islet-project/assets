#
# Copyright (c) 2015-2020, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Convenience function for adding build definitions
# $(eval $(call add_define,BAR_DEFINES,FOO)) will have:
# -DFOO if $(FOO) is empty; -DFOO=$(FOO) otherwise
# inside the BAR_DEFINES variable.
define add_define
$(1) += -D$(2)$(if $(value $(2)),=$(value $(2)),)
endef

# Convenience function for verifying option has a boolean value
# $(eval $(call assert_boolean,FOO)) will assert FOO is 0 or 1
define assert_boolean
$(and $(patsubst 0,,$(value $(1))),$(patsubst 1,,$(value $(1))),$(error $(1) must be boolean))
endef

# CREATE_SEQ is a recursive function to create sequence of numbers from 1 to
# $(2) and assign the sequence to $(1)
define CREATE_SEQ
$(if $(word $(2), $($(1))),\
  $(eval $(1) += $(words $($(1))))\
  $(eval $(1) := $(filter-out 0,$($(1)))),\
  $(eval $(1) += $(words $($(1))))\
  $(call CREATE_SEQ,$(1),$(2))\
)
endef
