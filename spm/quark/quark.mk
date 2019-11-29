#
# Copyright (c) 2018-2019, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include lib/sprt/sprt_client.mk

QUARK_DTB		:= $(BUILD_PLAT)/quark.dtb

QUARK_INCLUDES :=					\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/sprt				\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/runtime_services			\
	-Iinclude/runtime_services/secure_el0_payloads	\
	-Ispm/quark					\
	-Ispm/common					\
	${SPRT_LIB_INCLUDES}

QUARK_SOURCES	:=					\
	$(addprefix spm/quark/,				\
		aarch64/quark_entrypoint.S		\
		quark_main.c				\
	)						\
	$(addprefix spm/common/,			\
		aarch64/sp_arch_helpers.S		\
		sp_helpers.c				\
	)						\

# TODO: Remove dependency on TFTF files.
QUARK_SOURCES	+=					\
	tftf/framework/debug.c				\
	tftf/framework/${ARCH}/asm_debug.S

QUARK_SOURCES	+= 	drivers/console/${ARCH}/dummy_console.S		\
			lib/locks/${ARCH}/spinlock.S			\
			lib/utils/mp_printf.c				\
			${SPRT_LIB_SOURCES}

QUARK_LINKERFILE	:=	spm/quark/quark.ld.S

QUARK_DEFINES	:=

$(eval $(call add_define,QUARK_DEFINES,DEBUG))
$(eval $(call add_define,QUARK_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,QUARK_DEFINES,PLAT_${PLAT}))
$(eval $(call add_define,QUARK_DEFINES,AARCH64))

$(QUARK_DTB) : $(BUILD_PLAT)/quark $(BUILD_PLAT)/quark/quark.elf
$(QUARK_DTB) : spm/quark/quark.dts
	@echo "  DTBGEN  spm/quark/quark.dts"
	${Q}tools/generate_dtb/generate_dtb.sh \
		quark spm/quark/quark.dts $(BUILD_PLAT)
	@echo
	@echo "Built $@ successfully"
	@echo

quark: $(QUARK_DTB)
