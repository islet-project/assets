#
# Copyright (c) 2018-2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk

CACTUS_MM_INCLUDES :=					\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/drivers				\
	-Iinclude/drivers/arm				\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/runtime_services			\
	-Iinclude/runtime_services/secure_el0_payloads	\
	-Ispm/cactus_mm					\
	-Ispm/common					\

CACTUS_MM_SOURCES	:=				\
	$(addprefix spm/cactus_mm/,			\
		aarch64/cactus_mm_entrypoint.S		\
		cactus_mm_main.c			\
		cactus_mm_service_loop.c		\
		cactus_mm_tests_memory_attributes.c	\
		cactus_mm_tests_misc.c			\
		cactus_mm_tests_system_setup.c		\
	)						\
	$(addprefix spm/common/,			\
		aarch64/sp_arch_helpers.S		\
		sp_helpers.c				\
	)						\

# TODO: Remove dependency on TFTF files.
CACTUS_MM_SOURCES	+=				\
	tftf/framework/debug.c				\

CACTUS_MM_SOURCES	+=				\
	drivers/arm/pl011/${ARCH}/pl011_console.S	\
	drivers/console/console.c			\
	lib/${ARCH}/cache_helpers.S			\
	lib/${ARCH}/misc_helpers.S			\

CACTUS_MM_LINKERFILE	:=	spm/cactus_mm/cactus_mm.ld.S

CACTUS_MM_DEFINES	:=

# TODO: Assertions are disabled because they add several TFTF files as
# dependencies. It is needed to break the dependencies and remove this line when
# that is done.
CACTUS_MM_DEFINES	+= -DENABLE_ASSERTIONS=0

$(eval $(call add_define,CACTUS_MM_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,CACTUS_MM_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,CACTUS_MM_DEFINES,DEBUG))
$(eval $(call add_define,CACTUS_MM_DEFINES,ENABLE_BTI))
$(eval $(call add_define,CACTUS_MM_DEFINES,ENABLE_PAUTH))
$(eval $(call add_define,CACTUS_MM_DEFINES,LOG_LEVEL))
$(eval $(call add_define,CACTUS_MM_DEFINES,PLAT_${PLAT}))

