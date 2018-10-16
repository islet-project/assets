#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

CC		:=	${CROSS_COMPILE}gcc
AS		:=	${CROSS_COMPILE}as
LD		:=	${CROSS_COMPILE}ld
OC		:=	${CROSS_COMPILE}objcopy
OD		:=	${CROSS_COMPILE}objdump

PLAT		?=	fvp

ASFLAGS		:=	-nostdinc -ffreestanding -Wa,--fatal-warnings -Werror
ASFLAGS		+=	-Iplat/${PLAT}/ -I.

PLAT_BUILD_DIR	:=	build/${PLAT}
SOURCES		:=	entrypoint.S spin.S uart.S plat/${PLAT}/platform.S
OBJS		:=	$(patsubst %,$(PLAT_BUILD_DIR)/%,$(notdir $(SOURCES:.S=.o)))

PROGRAM		:=	el3_payload
LINKER_SCRIPT	:=	${PLAT_BUILD_DIR}/${PROGRAM}.ld
ELF		:=	${PLAT_BUILD_DIR}/${PROGRAM}.elf
BIN		:=	${PLAT_BUILD_DIR}/${PROGRAM}.bin
DUMP		:=	${PLAT_BUILD_DIR}/${PROGRAM}.dump

include plat/${PLAT}/platform.mk

all: ${BIN}

${PLAT_BUILD_DIR}:
	mkdir -p $@

${PLAT_BUILD_DIR}/%.o: %.S ${PLAT_BUILD_DIR}
	@echo "  CC      $<"
	${CC} ${ASFLAGS} -c $< -o $@

# Specific rule for this '.o' file to avoid worrying about
# plat/${PLAT}/platform.S being in a sub-directory...
# TODO: Fix this workaround.
${PLAT_BUILD_DIR}/platform.o: plat/${PLAT}/platform.S ${PLAT_BUILD_DIR}
	@echo "  CC      $<"
	${CC} ${ASFLAGS} -c $< -o $@

${PLAT_BUILD_DIR}/%.ld: %.ld.S ${PLAT_BUILD_DIR}
	@echo "  PP      $<"
	${CC} -DDRAM_BASE=${DRAM_BASE} -DDRAM_SIZE=${DRAM_SIZE} -E -P -o $@ $<

${ELF}: ${LINKER_SCRIPT} ${OBJS}
	@echo "  LD      $<"
	${LD} ${LDFLAGS} ${OBJS} --script ${LINKER_SCRIPT} -o $@

${BIN}: ${ELF}
	@echo "  BIN     $@"
	${OC} -O binary $< $@

${DUMP}: $(ELF)
	@echo "  OD      $$@"
	${OD} -dx $< > $@

dump: ${DUMP}

clean:
	rm -rf ${PLAT_BUILD_DIR}

distclean: clean
	rm -rf build/

run: run_${PLAT}

run_juno: ${ELF} scripts/juno/run_ds5_script.sh scripts/juno/load_el3_payload.ds
	scripts/juno/run_ds5_script.sh scripts/juno/load_el3_payload.ds

run_fvp: scripts/fvp/run_fvp.sh
	scripts/fvp/run_fvp.sh

help:
	@echo "EL3 test payload"
	@echo
	@echo "To build:"
	@echo "make [PLAT=fvp|juno] [TARGETS]"
	@echo ""
	@echo "The default platform is fvp."
	@echo
	@echo "TARGETS:"
	@echo "  all		Build the payload  [default target]"
	@echo "  dump		Generate a dump file of the program"
	@echo "  run		Run the payload on the given platform."
	@echo "         	/!\ For Juno, requires a connection to a Juno board via DSTREAM"
