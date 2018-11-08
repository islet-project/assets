#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

SPRT_LIB_SOURCES	:=	$(addprefix lib/sprt/,			\
					${ARCH}/sprt_client_helpers.S	\
					sprt_client.c			\
					sprt_queue.c)

SPRT_LIB_INCLUDES	:=	-Iinclude/lib/sprt/
