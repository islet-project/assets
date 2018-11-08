#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include lib/sprt/sprt_client.mk

CACTUS_DTB	:= $(BUILD_PLAT)/cactus.dtb

CACTUS_INCLUDES :=					\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/drivers				\
	-Iinclude/drivers/arm				\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/stdlib				\
	-Iinclude/lib/stdlib/sys			\
	-Iinclude/lib/sprt				\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/runtime_services			\
	-Iinclude/runtime_services/secure_el0_payloads	\
	-Ispm/cactus					\
	-Ispm/common					\
	${SPRT_LIB_INCLUDES}

CACTUS_SOURCES	:=					\
	$(addprefix spm/cactus/,			\
		aarch64/cactus_entrypoint.S		\
		cactus_main.c				\
		cactus_tests_memory_attributes.c	\
		cactus_tests_misc.c			\
		cactus_tests_system_setup.c		\
	)						\
	$(addprefix spm/common/,			\
		aarch64/sp_arch_helpers.S		\
		sp_helpers.c				\
	)						\

STDLIB_SOURCES	:=	$(addprefix lib/stdlib/,	\
	assert.c					\
	mem.c						\
	putchar.c					\
	printf.c					\
	rand.c						\
	strlen.c					\
	subr_prf.c					\
)

# TODO: Remove dependency on TFTF files.
CACTUS_SOURCES	+=					\
	tftf/framework/debug.c				\
	tftf/framework/${ARCH}/asm_debug.S

CACTUS_SOURCES	+= 	drivers/arm/pl011/${ARCH}/pl011_console.S	\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/locks/${ARCH}/spinlock.S			\
			lib/utils/mp_printf.c				\
			${STDLIB_SOURCES}				\
			${SPRT_LIB_SOURCES}

CACTUS_LINKERFILE	:=	spm/cactus/cactus.ld.S

CACTUS_DEFINES	:=

$(eval $(call add_define,CACTUS_DEFINES,DEBUG))
$(eval $(call add_define,CACTUS_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,CACTUS_DEFINES,LOG_LEVEL))
$(eval $(call add_define,CACTUS_DEFINES,PLAT_${PLAT}))
ifeq (${ARCH},aarch32)
        $(eval $(call add_define,CACTUS_DEFINES,AARCH32))
else
        $(eval $(call add_define,CACTUS_DEFINES,AARCH64))
endif

$(CACTUS_DTB) : $(BUILD_PLAT)/cactus $(BUILD_PLAT)/cactus/cactus.elf
$(CACTUS_DTB) : spm/cactus/cactus.dts
	@echo "  DTBGEN  spm/cactus/cactus.dts"
	${Q}tools/generate_dtb/generate_dtb.sh \
		cactus spm/cactus/cactus.dts $(BUILD_PLAT)
	@echo
	@echo "Built $@ successfully"
	@echo

cactus: $(CACTUS_DTB) $(AUTOGEN_DIR)/tests_list.h
