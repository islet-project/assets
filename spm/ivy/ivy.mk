#
# Copyright (c) 2018-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk
include lib/xlat_tables_v2/xlat_tables.mk

# Include ivy platform Makefile
IVY_PLAT_PATH	:= $(shell find spm/ivy/app/plat -wholename '*/${PLAT}')
ifneq (${IVY_PLAT_PATH},)
	include ${IVY_PLAT_PATH}/platform.mk
endif

IVY_DTB		:= build/${PLAT}/debug/ivy.dtb

IVY_INCLUDES :=					\
	-Itftf/framework/include			\
	-Iinclude					\
	-Iinclude/common				\
	-Iinclude/common/${ARCH}			\
	-Iinclude/lib					\
	-Iinclude/lib/${ARCH}				\
	-Iinclude/lib/utils				\
	-Iinclude/lib/xlat_tables			\
	-Iinclude/runtime_services			\
	-Iinclude/runtime_services/secure_el0_payloads	\
	-Ispm/ivy/app				\
	-Ispm/ivy/shim					\
	-Ispm/common

IVY_SOURCES	:=					\
	$(addprefix spm/ivy/app/,			\
		aarch64/ivy_entrypoint.S		\
		ivy_main.c				\
	)						\
	$(addprefix spm/ivy/shim/,			\
		aarch64/spm_shim_entrypoint.S		\
		aarch64/spm_shim_exceptions.S		\
		shim_main.c				\
	)						\
	$(addprefix spm/common/,			\
		aarch64/sp_arch_helpers.S		\
		sp_debug.c				\
		sp_helpers.c				\
		spm_helpers.c				\
	)						\

# TODO: Remove dependency on TFTF files.
IVY_SOURCES	+=					\
	tftf/framework/debug.c				\
	tftf/framework/${ARCH}/asm_debug.S		\
	tftf/tests/runtime_services/secure_service/ffa_helpers.c

IVY_SOURCES	+= 	drivers/arm/pl011/${ARCH}/pl011_console.S	\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/smc/${ARCH}/asm_smc.S			\
			lib/smc/${ARCH}/smc.c				\
			lib/smc/${ARCH}/hvc.c				\
			lib/locks/${ARCH}/spinlock.S			\
			lib/utils/mp_printf.c				\
			${XLAT_TABLES_LIB_SRCS}

IVY_LINKERFILE	:=	spm/ivy/ivy.ld.S

IVY_DEFINES	:=

$(eval $(call add_define,IVY_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,IVY_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,IVY_DEFINES,DEBUG))
$(eval $(call add_define,IVY_DEFINES,ENABLE_ASSERTIONS))
$(eval $(call add_define,IVY_DEFINES,ENABLE_BTI))
$(eval $(call add_define,IVY_DEFINES,ENABLE_PAUTH))
$(eval $(call add_define,IVY_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,IVY_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,IVY_DEFINES,FVP_MAX_PE_PER_CPU))
$(eval $(call add_define,IVY_DEFINES,LOG_LEVEL))
$(eval $(call add_define,IVY_DEFINES,PLAT_${PLAT}))

$(IVY_DTB) : $(BUILD_PLAT)/ivy $(BUILD_PLAT)/ivy/ivy.elf
$(IVY_DTB) : $(IVY_DTS)
	@echo "  DTBGEN  $@"
	${Q}tools/generate_dtb/generate_dtb.sh \
		ivy ${IVY_DTS} $(BUILD_PLAT)
	${Q}tools/generate_json/generate_json.sh \
		ivy $(BUILD_PLAT)
	@echo
	@echo "Built $@ successfully"
	@echo

ivy: $(IVY_DTB)

# FDTS_CP copies flattened device tree sources
#   $(1) = output directory
#   $(2) = flattened device tree source file to copy
define FDTS_CP
        $(eval FDTS := $(addprefix $(1)/,$(notdir $(2))))
FDTS_LIST += $(FDTS)
$(FDTS): $(2) $(IVY_DTB)
	@echo "  CP      $$<"
	${Q}cp $$< $$@
endef

ifdef FDTS_CP_LIST
        $(eval files := $(filter %.dts,$(FDTS_CP_LIST)))
        $(eval $(foreach file,$(files),$(call FDTS_CP,$(BUILD_PLAT),$(file))))
ivy: $(FDTS_LIST)
endif
