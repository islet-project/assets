#
# Copyright (c) 2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include branch_protection.mk

REALM_INCLUDES :=							\
	-Itftf/framework/include					\
	-Iinclude							\
	-Iinclude/common						\
	-Iinclude/common/${ARCH}					\
	-Iinclude/lib							\
	-Iinclude/lib/${ARCH}						\
	-Iinclude/lib/utils						\
	-Iinclude/lib/xlat_tables					\
	-Iinclude/runtime_services					\
	-Iinclude/runtime_services/host_realm_managment			\
	-Irealm								\
	-Irealm/aarch64

REALM_SOURCES:=								\
	$(addprefix realm/,						\
	aarch64/realm_entrypoint.S					\
	aarch64/realm_exceptions.S					\
	realm_debug.c							\
	realm_payload_main.c						\
	realm_interrupt.c						\
	realm_rsi.c							\
	realm_shared_data.c						\
	)

REALM_SOURCES += lib/${ARCH}/cache_helpers.S				\
	lib/${ARCH}/misc_helpers.S					\
	lib/smc/${ARCH}/asm_smc.S					\
	lib/smc/${ARCH}/smc.c						\
	lib/exceptions/${ARCH}/sync.c					\
	lib/locks/${ARCH}/spinlock.S					\
	lib/delay/delay.c

# TODO: Remove dependency on TFTF files.
REALM_SOURCES	+=							\
	tftf/framework/${ARCH}/exception_report.c

REALM_LINKERFILE:=	realm/realm.ld.S

REALM_DEFINES:=
$(eval $(call add_define,REALM_DEFINES,ARM_ARCH_MAJOR))
$(eval $(call add_define,REALM_DEFINES,ARM_ARCH_MINOR))
$(eval $(call add_define,REALM_DEFINES,ENABLE_BTI))
$(eval $(call add_define,REALM_DEFINES,ENABLE_PAUTH))
$(eval $(call add_define,REALM_DEFINES,LOG_LEVEL))
$(eval $(call add_define,REALM_DEFINES,IMAGE_REALM))
