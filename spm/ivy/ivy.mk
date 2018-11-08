#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include lib/sprt/sprt_client.mk

IVY_DTB		:= $(BUILD_PLAT)/ivy.dtb

IVY_INCLUDES :=					\
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
	-Ispm/ivy					\
	-Ispm/common					\
	${SPRT_LIB_INCLUDES}

IVY_SOURCES	:=					\
	$(addprefix spm/ivy/,			\
		aarch64/ivy_entrypoint.S		\
		ivy_main.c				\
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
IVY_SOURCES	+=					\
	tftf/framework/debug.c				\
	tftf/framework/${ARCH}/asm_debug.S

IVY_SOURCES	+= 	drivers/arm/pl011/${ARCH}/pl011_console.S	\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/locks/${ARCH}/spinlock.S			\
			lib/utils/mp_printf.c				\
			${STDLIB_SOURCES}				\
			${SPRT_LIB_SOURCES}

IVY_LINKERFILE	:=	spm/ivy/ivy.ld.S

IVY_DEFINES	:=

$(eval $(call add_define,IVY_DEFINES,DEBUG))
$(eval $(call add_define,IVY_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,IVY_DEFINES,LOG_LEVEL))
$(eval $(call add_define,IVY_DEFINES,PLAT_${PLAT}))
ifeq (${ARCH},aarch32)
        $(eval $(call add_define,IVY_DEFINES,AARCH32))
else
        $(eval $(call add_define,IVY_DEFINES,AARCH64))
endif

$(IVY_DTB) : $(BUILD_PLAT)/ivy $(BUILD_PLAT)/ivy/ivy.elf
$(IVY_DTB) : spm/ivy/ivy.dts
	@echo "  DTBGEN  spm/ivy/ivy.dts"
	${Q}tools/generate_dtb/generate_dtb.sh \
		ivy spm/ivy/ivy.dts $(BUILD_PLAT)
	@echo
	@echo "Built $@ successfully"
	@echo

ivy: $(IVY_DTB) $(AUTOGEN_DIR)/tests_list.h
