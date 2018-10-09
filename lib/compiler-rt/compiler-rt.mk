#
# Copyright (c) 2018, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

ifeq (${ARCH},aarch32)
COMPILER_RT_SRCS	:=	$(addprefix lib/compiler-rt/builtins/,	\
	arm/aeabi_uldivmod.S						\
	ctzdi2.c							\
	udivmoddi4.c							\
)
endif
