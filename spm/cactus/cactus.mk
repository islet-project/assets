#
# Copyright (c) 2018-2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk
include lib/xlat_tables_v2/xlat_tables.mk

CACTUS_DTB	:= $(BUILD_PLAT)/cactus.dtb

CACTUS_INCLUDES :=					\
	-Itftf/framework/include			\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/runtime_services			\
	-Ispm/cactus					\
	-Ispm/common					\

CACTUS_SOURCES	:=					\
	$(addprefix spm/cactus/,			\
		aarch64/cactus_entrypoint.S		\
		cactus_debug.c				\
		cactus_ffa_tests.c 			\
		cactus_main.c				\
	)						\
	$(addprefix spm/common/,			\
		aarch64/sp_arch_helpers.S		\
		sp_helpers.c				\
	)						\

# TODO: Remove dependency on TFTF files.
CACTUS_SOURCES	+=					\
	tftf/framework/debug.c				\
	tftf/framework/${ARCH}/asm_debug.S		\
	tftf/tests/runtime_services/secure_service/ffa_helpers.c

CACTUS_SOURCES	+= 	drivers/arm/pl011/${ARCH}/pl011_console.S	\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/smc/${ARCH}/asm_smc.S			\
			lib/smc/${ARCH}/smc.c				\
			lib/smc/${ARCH}/hvc.c				\
			lib/locks/${ARCH}/spinlock.S			\
			lib/utils/mp_printf.c				\
			${XLAT_TABLES_LIB_SRCS}

CACTUS_LINKERFILE	:=	spm/cactus/cactus.ld.S

CACTUS_DEFINES	:=

$(eval $(call add_define,CACTUS_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,CACTUS_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,CACTUS_DEFINES,DEBUG))
$(eval $(call add_define,CACTUS_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,CACTUS_DEFINES,ENABLE_BTI))
$(eval $(call add_define,CACTUS_DEFINES,ENABLE_PAUTH))
$(eval $(call add_define,CACTUS_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,CACTUS_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,CACTUS_DEFINES,FVP_MAX_PE_PER_CPU))
$(eval $(call add_define,CACTUS_DEFINES,LOG_LEVEL))
$(eval $(call add_define,CACTUS_DEFINES,PLAT_${PLAT}))


$(CACTUS_DTB) : $(BUILD_PLAT)/cactus $(BUILD_PLAT)/cactus/cactus.elf
$(CACTUS_DTB) : spm/cactus/cactus.dts
	@echo "  DTBGEN  spm/cactus/cactus.dts"
	${Q}tools/generate_dtb/generate_dtb.sh \
		cactus spm/cactus/cactus.dts $(BUILD_PLAT)
	${Q}tools/generate_json/generate_json.sh \
		cactus $(PLAT) $(BUILD_TYPE)
	@echo
	@echo "Built $@ successfully"
	@echo

cactus: $(CACTUS_DTB)
