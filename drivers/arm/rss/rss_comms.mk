#
# Copyright (c) 2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

$(warning "RSS driver is an experimental feature")

RSS_COMMS_SOURCES	:=	$(addprefix drivers/arm/rss/,			\
					rss_comms.c				\
					rss_comms_protocol.c			\
					rss_comms_protocol_embed.c		\
					rss_comms_protocol_pointer_access.c	\
				)

ifneq (${PLAT_RSS_COMMS_USE_SERIAL},0)
RSS_COMMS_SOURCES	+=	$(addprefix drivers/arm/rss/,			\
					rss_comms_serial.c			\
				)
else
RSS_COMMS_SOURCES	+=	$(addprefix drivers/arm/mhu/,			\
					mhu_v2_x.c				\
					mhu_wrapper_v2_x.c			\
				)
endif

PLAT_INCLUDES		+=	-Idrivers/arm/rss		\
				-Idrivers/arm/mhu
