#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include lib/xlat_tables_v2/xlat_tables.mk
include lib/compiler-rt/compiler-rt.mk

NS_BL2U_INCLUDES :=					\
	-I${AUTOGEN_DIR}				\
	-Itftf/framework/include			\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/drivers				\
	-Iinclude/drivers/arm				\
	-Iinclude/drivers/io				\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/stdlib				\
	-Iinclude/lib/stdlib/sys			\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/plat/common				\
	-Iinclude/runtime_services

# TODO: Remove dependency on TFTF files.
NS_BL2U_SOURCES := $(addprefix tftf/framework/,		\
	${ARCH}/arch.c					\
	${ARCH}/asm_debug.S				\
	debug.c						\
)

NS_BL2U_SOURCES	+=	fwu/ns_bl2u/${ARCH}/ns_bl2u_entrypoint.S	\
			fwu/ns_bl2u/ns_bl2u_main.c			\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/exception_stubs.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/locks/${ARCH}/spinlock.S			\
			lib/smc/${ARCH}/asm_smc.S			\
			lib/smc/${ARCH}/smc.c				\
			${STD_LIB_SOURCES}				\
			lib/utils/mp_printf.c				\
			lib/utils/uuid.c				\
			${XLAT_TABLES_LIB_SRCS}				\
			plat/arm/common/arm_fwu_io_storage.c		\
			plat/common/${ARCH}/platform_up_stack.S 	\
			plat/common/fwu_nvm_accessors.c			\
			plat/common/plat_common.c			\
			drivers/io/io_memmap.c				\
			drivers/io/io_fip.c

NS_BL2U_SOURCES	+=	${COMPILER_RT_SRCS}

NS_BL2U_LINKERFILE	:=	fwu/ns_bl2u/ns_bl2u.ld.S

# NS_BL2U requires accessing the flash. Force-enable it.
NS_BL2U_DEFINES := -DUSE_NVM=1

$(eval $(call add_define,NS_BL2U_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,NS_BL2U_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,NS_BL2U_DEFINES,DEBUG))
$(eval $(call add_define,NS_BL2U_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,NS_BL2U_DEFINES,FWU_BL_TEST))
$(eval $(call add_define,NS_BL2U_DEFINES,LOG_LEVEL))
$(eval $(call add_define,NS_BL2U_DEFINES,PLAT_${PLAT}))
ifeq (${ARCH},aarch32)
        $(eval $(call add_define,NS_BL2U_DEFINES,AARCH32))
else
        $(eval $(call add_define,NS_BL2U_DEFINES,AARCH64))
endif

ns_bl2u: ${AUTOGEN_DIR}/tests_list.h
