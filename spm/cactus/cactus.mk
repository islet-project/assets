#
# Copyright (c) 2018-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk
include lib/xlat_tables_v2/xlat_tables.mk

# Include cactus platform make file
CACTUS_PLAT_PATH	:= $(shell find spm/cactus/plat -wholename '*/${PLAT}')
ifneq (${CACTUS_PLAT_PATH},)
	include ${CACTUS_PLAT_PATH}/platform.mk
endif

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
	-Iinclude/plat/common				\
	-Iinclude/runtime_services			\
	-Ispm/cactus					\
	-Ispm/common

CACTUS_SOURCES	:=					\
	$(addprefix spm/cactus/,			\
		aarch64/cactus_entrypoint.S		\
		aarch64/cactus_exceptions.S		\
		cactus_interrupt.c			\
		cactus_main.c				\
	)						\
	$(addprefix spm/common/,			\
		aarch64/sp_arch_helpers.S		\
		sp_debug.c				\
		sp_helpers.c				\
		spm_helpers.c				\
	)						\
	$(addprefix spm/cactus/cactus_tests/,		\
		cactus_message_loop.c			\
		cactus_test_cpu_features.c		\
		cactus_test_direct_messaging.c		\
		cactus_test_ffa.c 			\
		cactus_test_interrupts.c		\
		cactus_test_memory_sharing.c		\
		cactus_tests_smmuv3.c			\
		cactus_test_notifications.c		\
	)

# TODO: Remove dependency on TFTF files.
CACTUS_SOURCES	+=					\
	tftf/framework/debug.c				\
	tftf/framework/${ARCH}/asm_debug.S		\
	tftf/tests/runtime_services/secure_service/ffa_helpers.c \
	tftf/tests/runtime_services/secure_service/spm_common.c	\
	tftf/framework/${ARCH}/exception_report.c

CACTUS_SOURCES	+= 	drivers/arm/pl011/${ARCH}/pl011_console.S	\
			drivers/arm/sp805/sp805.c			\
			lib/${ARCH}/cache_helpers.S			\
			lib/${ARCH}/misc_helpers.S			\
			lib/smc/${ARCH}/asm_smc.S			\
			lib/smc/${ARCH}/smc.c				\
			lib/smc/${ARCH}/hvc.c				\
			lib/exceptions/${ARCH}/sync.c			\
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
$(eval $(call add_define,CACTUS_DEFINES,LOG_LEVEL))
$(eval $(call add_define,CACTUS_DEFINES,PLAT_${PLAT}))
$(eval $(call add_define,CACTUS_DEFINES,PLAT_XLAT_TABLES_DYNAMIC))

$(CACTUS_DTB) : $(BUILD_PLAT)/cactus $(BUILD_PLAT)/cactus/cactus.elf
$(CACTUS_DTB) : $(CACTUS_DTS)
	@echo "  DTBGEN  $@"
	${Q}tools/generate_dtb/generate_dtb.sh \
		cactus ${CACTUS_DTS} $(BUILD_PLAT)
	${Q}tools/generate_json/generate_json.sh \
		cactus $(BUILD_PLAT)
	@echo
	@echo "Built $@ successfully"
	@echo

cactus: $(CACTUS_DTB)

# FDTS_CP copies flattened device tree sources
#   $(1) = output directory
#   $(2) = flattened device tree source file to copy
define FDTS_CP
        $(eval FDTS := $(addprefix $(1)/,$(notdir $(2))))
FDTS_LIST += $(FDTS)
$(FDTS): $(2) $(CACTUS_DTB)
	@echo "  CP      $$<"
	${Q}cp $$< $$@
endef

ifdef FDTS_CP_LIST
        $(eval files := $(filter %.dts,$(FDTS_CP_LIST)))
        $(eval $(foreach file,$(files),$(call FDTS_CP,$(BUILD_PLAT),$(file))))
cactus: $(FDTS_LIST)
endif
