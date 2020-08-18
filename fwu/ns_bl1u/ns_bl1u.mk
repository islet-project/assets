#
# Copyright (c) 2018-2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk
include lib/xlat_tables_v2/xlat_tables.mk
include lib/compiler-rt/compiler-rt.mk

NS_BL1U_INCLUDES := 					\
	-I${AUTOGEN_DIR}				\
	-Itftf/framework/include			\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/extensions			\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/plat/common				\
	-Iinclude/runtime_services

# TODO: Remove dependency on TFTF files.
NS_BL1U_SOURCES := $(addprefix tftf/framework/,		\
	${ARCH}/arch.c					\
	${ARCH}/asm_debug.S				\
	debug.c						\
)

NS_BL1U_SOURCES	+=	drivers/io/io_fip.c				\
			drivers/io/io_memmap.c				\
			fwu/ns_bl1u/${ARCH}/ns_bl1u_entrypoint.S	\
			fwu/ns_bl1u/ns_bl1u_main.c			\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/exception_stubs.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/locks/${ARCH}/spinlock.S			\
			lib/smc/${ARCH}/asm_smc.S			\
			lib/smc/${ARCH}/smc.c				\
			lib/utils/mp_printf.c				\
			lib/utils/uuid.c				\
			${XLAT_TABLES_LIB_SRCS}				\
			plat/arm/common/arm_fwu_io_storage.c		\
			plat/common/${ARCH}/platform_up_stack.S 	\
			plat/common/image_loader.c			\
			plat/common/plat_common.c

NS_BL1U_SOURCES	+=	${COMPILER_RT_SRCS}

ifeq (${FWU_BL_TEST},1)
	NS_BL1U_SOURCES	+= fwu/ns_bl1u/ns_bl1u_tests.c
endif

ifeq (${ENABLE_PAUTH},1)
# ARMv8.3 Pointer Authentication support files
NS_BL1U_SOURCES	+=	lib/extensions/pauth/aarch64/pauth.c		\
			lib/extensions/pauth/aarch64/pauth_helpers.S
endif

NS_BL1U_LINKERFILE	:=	fwu/ns_bl1u/ns_bl1u.ld.S

# NS_BL1U requires accessing the flash. Force-enable it.
NS_BL1U_DEFINES	:= -DUSE_NVM=1

$(eval $(call add_define,NS_BL1U_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,NS_BL1U_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,NS_BL1U_DEFINES,DEBUG))
$(eval $(call add_define,NS_BL1U_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,NS_BL1U_DEFINES,ENABLE_BTI))
$(eval $(call add_define,NS_BL1U_DEFINES,ENABLE_PAUTH))
$(eval $(call add_define,NS_BL1U_DEFINES,FWU_BL_TEST))
$(eval $(call add_define,NS_BL1U_DEFINES,LOG_LEVEL))
$(eval $(call add_define,NS_BL1U_DEFINES,PLAT_${PLAT}))
