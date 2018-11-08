#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

AUTOGEN_DIR		:=	$(BUILD_PLAT)/autogen

include lib/xlat_tables_v2/xlat_tables.mk
include lib/compiler-rt/compiler-rt.mk

TFTF_INCLUDES	:= 					\
	-I${AUTOGEN_DIR} 				\
	-Itftf/framework/include			\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/drivers				\
	-Iinclude/drivers/arm				\
	-Iinclude/drivers/io				\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/extensions			\
	-Iinclude/lib/stdlib				\
	-Iinclude/lib/stdlib/sys			\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/plat/common				\
	-Iinclude/runtime_services			\
	-Iinclude/runtime_services/secure_el0_payloads	\
	-Iinclude/runtime_services/secure_el1_payloads	\
	-Ispm/cactus					\
	-Ispm/ivy

# Standard C library source files
STD_LIB_SOURCES		:=	lib/stdlib/abort.c			\
				lib/stdlib/assert.c			\
				lib/stdlib/mem.c			\
				lib/stdlib/printf.c			\
				lib/stdlib/putchar.c			\
				lib/stdlib/puts.c			\
				lib/stdlib/rand.c			\
				lib/stdlib/strchr.c			\
				lib/stdlib/strcmp.c			\
				lib/stdlib/strlen.c			\
				lib/stdlib/strncmp.c			\
				lib/stdlib/strncpy.c			\
				lib/stdlib/subr_prf.c

FRAMEWORK_SOURCES	:=	${AUTOGEN_DIR}/tests_list.c

FRAMEWORK_SOURCES	+=	$(addprefix tftf/,			\
	framework/${ARCH}/arch.c					\
	framework/${ARCH}/asm_debug.S					\
	framework/${ARCH}/entrypoint.S					\
	framework/${ARCH}/exceptions.S					\
	framework/debug.c						\
	framework/main.c						\
	framework/nvm_results_helpers.c					\
	framework/report.c						\
	framework/timer/timer_framework.c				\
	tests/common/test_helpers.c					\
)

FRAMEWORK_SOURCES	+=						\
	lib/${ARCH}/cache_helpers.S					\
	lib/${ARCH}/misc_helpers.S					\
	lib/delay/delay.c						\
	lib/events/events.c						\
	lib/extensions/amu/${ARCH}/amu.c				\
	lib/extensions/amu/${ARCH}/amu_helpers.S			\
	lib/irq/irq.c							\
	lib/locks/${ARCH}/spinlock.S					\
	lib/power_management/hotplug/hotplug.c				\
	lib/power_management/suspend/${ARCH}/asm_tftf_suspend.S		\
	lib/power_management/suspend/tftf_suspend.c			\
	lib/psci/psci.c							\
	lib/sdei/sdei.c							\
	lib/smc/${ARCH}/asm_smc.S					\
	lib/smc/${ARCH}/smc.c						\
	${STD_LIB_SOURCES}						\
	lib/trusted_os/trusted_os.c					\
	lib/utils/mp_printf.c						\
	lib/utils/uuid.c						\
	${XLAT_TABLES_LIB_SRCS}						\
	plat/common/${ARCH}/platform_mp_stack.S 			\
	plat/common/plat_common.c					\
	plat/common/plat_state_id.c					\
	plat/common/plat_topology.c					\
	plat/common/tftf_nvm_accessors.c


FRAMEWORK_SOURCES	+=	${COMPILER_RT_SRCS}

TFTF_LINKERFILE		:=	tftf/framework/tftf.ld.S


TFTF_DEFINES :=

# Enable dynamic translation tables
PLAT_XLAT_TABLES_DYNAMIC := 1
$(eval $(call add_define,TFTF_DEFINES,PLAT_XLAT_TABLES_DYNAMIC))
