/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Arm Limited.
 * All rights reserved.
 */
#include <libcflat.h>
#include <vmalloc.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>
#include <asm/mmu.h>
#include <asm/rsi.h>
#include <linux/compiler.h>
#include <alloc_page.h>
#include <asm/pgtable.h>

typedef void (*empty_fn)(void);

static bool test_passed;

/*
 * The virtual address of the page that the test has made the access to
 * in order to cause the I/DAbort with I/DFSC = Synchronous External Abort.
 */
static void* target_page_va;

/*
 * Ensure that the @va is the executable location from EL1:
 * - SCTLR_EL1.WXN must be off.
 * - Disable the access from EL0 (controlled by AP[1] in PTE).
 */
static void enable_instruction_fetch(void* va)
{
	unsigned long sctlr = read_sysreg(sctlr_el1);
	if (sctlr & SCTLR_EL1_WXN) {
		sctlr &= ~SCTLR_EL1_WXN;
		write_sysreg(sctlr, sctlr_el1);
		isb();
		flush_tlb_all();
	}

	mmu_clear_user(current_thread_info()->pgtable, (u64)va);
}

static void data_abort_handler(struct pt_regs *regs, unsigned int esr)
{
	if ((esr & ESR_EL1_FSC_MASK) == ESR_EL1_FSC_EXTABT)
		test_passed = true;

	report_info("esr = %x", esr);
	/*
	 * Advance the PC to complete the test.
	 */
	regs->pc += 4;
}

static void data_access_to_empty(void)
{
	test_passed = false;
	target_page_va = alloc_page();
	phys_addr_t empty_ipa = virt_to_phys(target_page_va);

	arm_set_memory_shared(empty_ipa, SZ_4K);

	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_DABT_EL1, data_abort_handler);
	READ_ONCE(((char*)target_page_va)[0x55]);
	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_DABT_EL1, NULL);

	report(test_passed, " ");
}

static void instruction_abort_handler(struct pt_regs *regs, unsigned int esr)
{
	if (((esr & ESR_EL1_FSC_MASK) == ESR_EL1_FSC_EXTABT) &&
	     (regs->pc == (u64)target_page_va))
		test_passed = true;

	report_info("esr = %x", esr);
	/*
	 * Simulate the RET instruction to complete the test.
	 */
	regs->pc = regs->regs[30];
}

static void instr_fetch_from_empty(void)
{
	phys_addr_t empty_ipa;

	test_passed = false;
	target_page_va = alloc_page();
	enable_instruction_fetch(target_page_va);

	empty_ipa = virt_to_phys((void*)target_page_va);

	arm_set_memory_shared(empty_ipa, SZ_4K);

	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_IABT_EL1, instruction_abort_handler);
	/*
	 * This should cause the IAbort with IFSC = SEA
	 */
	((empty_fn)target_page_va)();
	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_IABT_EL1, NULL);

	report(test_passed, " ");
}

static void instr_fetch_from_unprotected(void)
{
	test_passed = false;
	/*
	 * The test will attempt to execute an instruction from the start of
	 * the unprotected IPA space.
	 */
	target_page_va = vmap(PTE_NS_SHARED, SZ_4K);
	enable_instruction_fetch(target_page_va);

	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_IABT_EL1, instruction_abort_handler);
	/*
	 * This should cause the IAbort with IFSC = SEA
	 */
	((empty_fn)target_page_va)();
	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_IABT_EL1, NULL);

	report(test_passed, " ");
}

int main(int argc, char **argv)
{
	report_prefix_push("in_realm_sea");

	report_prefix_push("data_access_to_empty");
	data_access_to_empty();
	report_prefix_pop();

	report_prefix_push("instr_fetch_from_empty");
	instr_fetch_from_empty();
	report_prefix_pop();

	report_prefix_push("instr_fetch_from_unprotected");
	instr_fetch_from_unprotected();
	report_prefix_pop();

	return report_summary();
}
