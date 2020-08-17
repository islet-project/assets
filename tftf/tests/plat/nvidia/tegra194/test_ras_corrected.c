/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <events.h>
#include <lib/irq.h>
#include <power_management.h>
#include <test_helpers.h>
#include <tftf_lib.h>

#include <platform.h>

#include "include/tegra194_ras.h"

/* Macro to indicate CPU to start an action */
#define START				U(0xAA55)

/* Global flag to indicate that a fault was received */
static volatile uint64_t irq_received;

/* NVIDIA Pseudo fault generation registers */
#define T194_ERXPFGCTL_EL1		S3_0_C15_C1_4
#define T194_ERXPFGCDN_EL1		S3_0_C15_C1_6
DEFINE_RENAME_SYSREG_RW_FUNCS(erxpfgctl_el1, T194_ERXPFGCTL_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(erxpfgcdn_el1, T194_ERXPFGCDN_EL1)

/* Instantiate RAS nodes */
PER_CORE_RAS_NODE_LIST(DEFINE_ONE_RAS_NODE);
PER_CLUSTER_RAS_NODE_LIST(DEFINE_ONE_RAS_NODE);
SCF_L3_BANK_RAS_NODE_LIST(DEFINE_ONE_RAS_NODE);
CCPLEX_RAS_NODE_LIST(DEFINE_ONE_RAS_NODE);

/* Instantiate RAS node groups */
static __unused struct ras_aux_data per_core_ras_group[] = {
	PER_CORE_RAS_GROUP_NODES
};

static __unused struct ras_aux_data per_cluster_ras_group[] = {
	PER_CLUSTER_RAS_GROUP_NODES
};

static __unused struct ras_aux_data scf_l3_ras_group[] = {
	SCF_L3_BANK_RAS_GROUP_NODES
};

static __unused struct ras_aux_data ccplex_ras_group[] = {
	CCPLEX_RAS_GROUP_NODES
};

/*
 * we have same probe and handler for each error record group, use a macro to
 * simply the record definition.
 */
#define ADD_ONE_ERR_GROUP(errselr_start, group)					\
	{									\
		.sysreg.idx_start = (errselr_start),				\
		.sysreg.num_idx = ARRAY_SIZE((group)),				\
		.aux_data = (group)						\
	}

/* RAS error record group information */
static struct err_record_info tegra194_ras_records[] = {
	/*
	 * Per core ras error records
	 *
	 * ERRSELR starts from (0*256 + Logical_CPU_ID*16 + 0) to
	 * (0*256 + Logical_CPU_ID*16 + 5) for each group.
	 * 8 cores/groups, 6 * 8 nodes in total.
	 */
	ADD_ONE_ERR_GROUP(0x000, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x010, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x020, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x030, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x040, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x050, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x060, per_core_ras_group),
	ADD_ONE_ERR_GROUP(0x070, per_core_ras_group),

	/*
	 * Per cluster ras error records
	 *
	 * ERRSELR starts from 2*256 + Logical_Cluster_ID*16 + 0 to
	 * 2*256 + Logical_Cluster_ID*16 + 3.
	 * 4 clusters/groups, 3 * 4 nodes in total.
	 */
	ADD_ONE_ERR_GROUP(0x200, per_cluster_ras_group),
	ADD_ONE_ERR_GROUP(0x210, per_cluster_ras_group),
	ADD_ONE_ERR_GROUP(0x220, per_cluster_ras_group),
	ADD_ONE_ERR_GROUP(0x230, per_cluster_ras_group),

	/*
	 * SCF L3_Bank ras error records
	 *
	 * ERRSELR: 3*256 + L3_Bank_ID, L3_Bank_ID: 0-3
	 * 1 groups, 4 nodes in total.
	 */
	ADD_ONE_ERR_GROUP(0x300, scf_l3_ras_group),

	/*
	 * CCPLEX ras error records
	 *
	 * ERRSELR: 4*256 + Unit_ID, Unit_ID: 0 - 4
	 * 1 groups, 5 nodes in total.
	 */
	ADD_ONE_ERR_GROUP(0x400, ccplex_ras_group),
};

static void test_ras_inject_error(uint32_t errselr_el1, unsigned int errctlr_bit)
{
	uint64_t pfg_ctlr = BIT_64(errctlr_bit);

	INFO("Injecting on 0x%lx:\n\terrctlr_el1=%d\n\terrselr_el1=0x%x\n\tpfg_ctlr=0x%llx\n",
		read_mpidr_el1(), errctlr_bit, errselr_el1, pfg_ctlr);

	/* clear the flag before we inject error */
	irq_received = 0;
	dccvac((uint64_t)&irq_received);
	dmbish();

	/* Choose error record */
	write_errselr_el1(errselr_el1);

	/* Program count down timer to 1 */
	write_erxpfgcdn_el1(1);

	/* Start count down to generate error on expiry */
	write_erxpfgctl_el1(3 << 6 | ERXPFGCTL_CDEN_BIT | pfg_ctlr);
	waitms(5);

	/* Wait until IRQ fires */
	do {
		/*
		 * clean+invalidate cache lines before reading the global
		 * flag populated by another CPU
		 */
		dccivac((uint64_t)&irq_received);
		dmbish();
	} while (irq_received == 0);

	/* write 1-to-clear */
	write_erxstatus_el1(read_erxstatus_el1() | (3 << 24));
}

static void generate_corrected_faults(void)
{
	unsigned int i;
	unsigned int j;
	unsigned int k;
	unsigned int total = 0;

	for (i = 0; i < ARRAY_SIZE(tegra194_ras_records); i++)
		total += tegra194_ras_records[i].sysreg.num_idx;

	VERBOSE("Total Nodes:%u\n", total);

	for (i = 0; i < ARRAY_SIZE(tegra194_ras_records); i++) {

		const struct err_record_info *info = &tegra194_ras_records[i];
		uint32_t idx_start = info->sysreg.idx_start;
		uint32_t num_idx = info->sysreg.num_idx;
		const struct ras_aux_data *aux_data =
		    (const struct ras_aux_data *)info->aux_data;

		/* No corrected errors for this node */
		if (idx_start == 0x400) {
			VERBOSE("0x%lx skipping errselr_el1=0x%x\n",
				read_mpidr_el1(), idx_start);
			continue;
		}

		for (j = 0; j < num_idx; j++) {
			uint32_t errselr_el1 = idx_start + j;
			uint64_t __unused err_fr;
			uint64_t uncorr_errs, corr_errs;

			/* Write to ERRSELR_EL1 to select the error record */
			write_errselr_el1(errselr_el1);

			/*
			 * all supported errors for this node exist in the
			 * top 32 bits
			 */
			err_fr = read_erxfr_el1();
			err_fr >>= 32;
			err_fr <<= 32;

			/*
			 * Mask the corrected errors that are disabled
			 * in the ERXFR register
			 */
			uncorr_errs = aux_data[j].err_ctrl();
			corr_errs = ~uncorr_errs & err_fr;

			for (k = 32; k < 64; k++) {
				/*
				 * JSR_MTS node, errctlr_bit = 32 or 34
				 * are uncorrected errors and should be
				 * skipped
				 */
				if ((idx_start < 0x200) && ((errselr_el1 & 0xF) == 2) && (k == 32 || k == 34)) {
					VERBOSE("0x%lx skipping errselr_el1=0x%x\n",
						read_mpidr_el1(), errselr_el1);
					continue;
				}

				if (corr_errs & BIT_64(k))
					test_ras_inject_error(errselr_el1, k);
			}
		}
	}
}

static int ce_irq_handler(void *data)
{
	unsigned int __unused irq_num = *(unsigned int *)data;

	/* write 1-to-clear */
	write_erxstatus_el1(read_erxstatus_el1() | (3 << 24));

	irq_received = 1;

	/*
	 * clean cache lines after writing the global flag so that
	 * latest value is visible to other CPUs
	 */
	dccvac((uint64_t)&irq_received);
	dsbish();

	/* Return value doesn't matter */
	return 0;
}

static event_t cpu_booted[PLATFORM_CORE_COUNT];
static volatile uint64_t cpu_powerdown[PLATFORM_CORE_COUNT];
static volatile uint64_t cpu_start_test[PLATFORM_CORE_COUNT];
static volatile uint64_t cpu_test_completed[PLATFORM_CORE_COUNT];

static test_result_t test_corrected_errors(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	VERBOSE("Hello from core 0x%x\n", mpid);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	/* Wait until lead CPU asks us to start the test */
	do {
		/*
		 * clean+invalidate cache lines before reading the global
		 * flag populated by another CPU
		 */
		dccivac((uintptr_t)&cpu_start_test[core_pos]);
		dmbish();
	} while (!cpu_start_test[core_pos]);

	generate_corrected_faults();

	VERBOSE("0x%lx: test complete\n", read_mpidr_el1());

	/* Inform lead CPU of test completion */
	cpu_test_completed[core_pos] = true;
	dccvac((uintptr_t)&cpu_test_completed[core_pos]);
	dsbish();

	/* Wait until lead CPU asks us to power down */
	do {
		/*
		 * clean+invalidate cache lines before reading the global
		 * flag populated by another CPU
		 */
		dccivac((uintptr_t)&cpu_powerdown[core_pos]);
		dmbish();
	} while (!cpu_powerdown[core_pos]);

	return TEST_RESULT_SUCCESS;
}

test_result_t test_ras_corrected(void)
{
	int64_t __unused ret = 0;
	unsigned int cpu_node, cpu_mpid;
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos;

	tftf_testcase_printf("Tegra194 corrected RAS error verification\n");

	/* long execution test; reset watchdog */
	tftf_platform_watchdog_reset();

	/* register IRQ handler */
	for (uint32_t irq = 424; irq <= 431; irq++) {

		ret = tftf_irq_register_handler(irq, ce_irq_handler);
		if (ret < 0)
			return TEST_RESULT_FAIL;

		/* enable the IRQ */
		tftf_irq_enable(irq, GIC_HIGHEST_NS_PRIORITY);
	}

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {

		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t) test_corrected_errors,
				0);
		if (ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;
	}

	/*
	 * The lead CPU needs to wait for all other CPUs to enter the test.
	 * This is because the test framework declares the end of a test when no
	 * CPU is in the test. Therefore, if the lead CPU goes ahead and exits
	 * the test then potentially there could be no CPU executing the test at
	 * this time because none of them have entered the test yet, hence the
	 * framework will be misled in thinking the test is finished.
	 */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_booted[core_pos]);
	}

	/* Ask all CPUs to start the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/*
		 * Except lead CPU, Wait for all cores to be powered off
		 * by framework
		 */
		if (cpu_mpid == lead_mpid)
			continue;

		/* Allow the CPU to start the test */
		core_pos = platform_get_core_pos(cpu_mpid);
		cpu_start_test[core_pos] = START;

		/*
		 * clean cache lines after writing the global flag so that
		 * latest value is visible to other CPUs
		 */
		dccvac((uintptr_t)&cpu_start_test[core_pos]);
		dsbish();

		/* Wait for the CPU to complete the test */
		do {
			/*
			 * clean+invalidate cache lines before reading the global
			 * flag populated by another CPU
			 */
			dccivac((uintptr_t)&cpu_test_completed[core_pos]);
			dmbish();
		} while (!cpu_test_completed[core_pos]);
	}

	/* run through all supported corrected faults */
	generate_corrected_faults();

	/* Wait for all CPUs to power off */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/*
		 * Except lead CPU, Wait for all cores to be powered off
		 * by framework
		 */
		if (cpu_mpid == lead_mpid)
			continue;

		/* Allow other CPUs to power down */
		core_pos = platform_get_core_pos(cpu_mpid);
		cpu_powerdown[core_pos] = START;

		/*
		 * clean cache lines after writing the global flag so that
		 * latest value is visible to other CPUs
		 */
		dccvac((uintptr_t)&cpu_powerdown[core_pos]);
		dsbish();

		/* Wait for the CPU to actually power off */
		while (tftf_psci_affinity_info(cpu_mpid, MPIDR_AFFLVL0) != PSCI_STATE_OFF)
			dsbsy();
	}

	return TEST_RESULT_SUCCESS;
}
