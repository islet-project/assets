#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Generate random fuzzing seeds
# If no instance count is provided, default to 1 instance
# If no seeds are provided, generate them randomly
# The number of seeds provided must match the instance count
SMC_FUZZ_INSTANCE_COUNT ?= 1
SMC_FUZZ_SEEDS ?= $(shell python -c "from random import randint; seeds = [randint(0, 4294967295) for i in range($(SMC_FUZZ_INSTANCE_COUNT))];print(\",\".join(str(x) for x in seeds));")
SMC_FUZZ_CALLS_PER_INSTANCE ?= 100

# Validate SMC fuzzer parameters

# Instance count must not be zero
ifeq ($(SMC_FUZZ_INSTANCE_COUNT),0)
$(error SMC_FUZZ_INSTANCE_COUNT must not be zero!)
endif

# Calls per instance must not be zero
ifeq ($(SMC_FUZZ_CALLS_PER_INSTANCE),0)
$(error SMC_FUZZ_CALLS_PER_INSTANCE must not be zero!)
endif

# Make sure seed count and instance count match
TEST_SEED_COUNT = $(shell python -c "print(len(\"$(SMC_FUZZ_SEEDS)\".split(\",\")))")
ifneq ($(TEST_SEED_COUNT), $(SMC_FUZZ_INSTANCE_COUNT))
$(error Number of seeds does not match SMC_FUZZ_INSTANCE_COUNT!)
endif

# Add definitions to TFTF_DEFINES so they can be used in the code
$(eval $(call add_define,TFTF_DEFINES,SMC_FUZZ_SEEDS))
$(eval $(call add_define,TFTF_DEFINES,SMC_FUZZ_INSTANCE_COUNT))
$(eval $(call add_define,TFTF_DEFINES,SMC_FUZZ_CALLS_PER_INSTANCE))

TESTS_SOURCES	+=							\
	$(addprefix smc_fuzz/src/,					\
		randsmcmod.c						\
		smcmalloc.c						\
		fifo3d.c						\
	)
