#
# Copyright (c) 2018-2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default number of threads per CPU on FVP
FVP_MAX_PE_PER_CPU		:= 1

# Check the PE per core count
ifneq ($(FVP_MAX_PE_PER_CPU),$(filter $(FVP_MAX_PE_PER_CPU),1 2))
$(error "Incorrect FVP_MAX_PE_PER_CPU = ${FVP_MAX_PE_PER_CPU} \
	specified for FVP port")
endif

# Default cluster count and number of CPUs per cluster for FVP
ifeq ($(FVP_MAX_PE_PER_CPU),1)
FVP_CLUSTER_COUNT		:= 2
FVP_MAX_CPUS_PER_CLUSTER	:= 4
else
FVP_CLUSTER_COUNT		:= 1
FVP_MAX_CPUS_PER_CLUSTER	:= 8
endif

# Check cluster count and number of CPUs per cluster
ifeq ($(FVP_MAX_PE_PER_CPU),2)
# Multithreaded CPU: 1 cluster with up to 8 CPUs
$(eval $(call CREATE_SEQ,CLS,1))
$(eval $(call CREATE_SEQ,CPU,8))
else
# CPU inside DynamIQ Shared Unit: 1 cluster with up to 8 CPUs
ifeq ($(FVP_CLUSTER_COUNT),1)
$(eval $(call CREATE_SEQ,CLS,1))
$(eval $(call CREATE_SEQ,CPU,8))
else
# CPU with single thread: max 4 clusters with up to 4 CPUs
$(eval $(call CREATE_SEQ,CLS,4))
$(eval $(call CREATE_SEQ,CPU,4))
endif
endif

# Check cluster count
ifneq ($(FVP_CLUSTER_COUNT),$(filter $(FVP_CLUSTER_COUNT),$(CLS)))
  $(error "Incorrect FVP_CLUSTER_COUNT = ${FVP_CLUSTER_COUNT} \
  specified for FVP port with \
  FVP_MAX_CPUS_PER_CLUSTER = ${FVP_MAX_CPUS_PER_CLUSTER} \
  FVP_MAX_PE_PER_CPU = ${FVP_MAX_PE_PER_CPU}")
endif

# Check number of CPUs per cluster
ifneq ($(FVP_MAX_CPUS_PER_CLUSTER),$(filter $(FVP_MAX_CPUS_PER_CLUSTER),$(CPU)))
  $(error "Incorrect FVP_MAX_CPUS_PER_CLUSTER = ${FVP_MAX_CPUS_PER_CLUSTER} \
  specified for FVP port with \
  FVP_CLUSTER_COUNT = ${FVP_CLUSTER_COUNT} \
  FVP_MAX_PE_PER_CPU = ${FVP_MAX_PE_PER_CPU}")
endif

# Pass FVP topology definitions to the build system
$(eval $(call add_define,CACTUS_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,CACTUS_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,CACTUS_DEFINES,FVP_MAX_PE_PER_CPU))

$(eval $(call add_define,CACTUS_MM_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,CACTUS_MM_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,CACTUS_MM_DEFINES,FVP_MAX_PE_PER_CPU))

$(eval $(call add_define,IVY_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,IVY_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,IVY_DEFINES,FVP_MAX_PE_PER_CPU))

$(eval $(call add_define,NS_BL1U_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,NS_BL1U_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,NS_BL1U_DEFINES,FVP_MAX_PE_PER_CPU))

$(eval $(call add_define,NS_BL2U_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,NS_BL2U_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,NS_BL2U_DEFINES,FVP_MAX_PE_PER_CPU))

$(eval $(call add_define,TFTF_DEFINES,FVP_CLUSTER_COUNT))
$(eval $(call add_define,TFTF_DEFINES,FVP_MAX_CPUS_PER_CLUSTER))
$(eval $(call add_define,TFTF_DEFINES,FVP_MAX_PE_PER_CPU))

PLAT_INCLUDES	+=	-Iplat/arm/fvp/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2v3.c			\
			drivers/arm/gic/gic_v2.c			\
			drivers/arm/gic/gic_v3.c			\
			drivers/arm/sp805/sp805.c			\
			drivers/arm/timer/private_timer.c		\
			drivers/arm/timer/system_timer.c		\
			plat/arm/fvp/${ARCH}/plat_helpers.S		\
			plat/arm/fvp/fvp_pwr_state.c			\
			plat/arm/fvp/fvp_topology.c			\
			plat/arm/fvp/fvp_mem_prot.c			\
			plat/arm/fvp/plat_setup.c

CACTUS_SOURCES	+=	plat/arm/fvp/${ARCH}/plat_helpers.S
IVY_SOURCES	+=	plat/arm/fvp/${ARCH}/plat_helpers.S

# Firmware update is implemented on FVP.
FIRMWARE_UPDATE := 1

PLAT_TESTS_SKIP_LIST	:=	plat/arm/fvp/fvp_tests_to_skip.txt

include plat/arm/common/arm_common.mk
