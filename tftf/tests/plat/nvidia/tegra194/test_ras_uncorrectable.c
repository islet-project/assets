/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <events.h>
#include <lib/irq.h>
#include <power_management.h>
#include <sdei.h>
#include <test_helpers.h>
#include <tftf_lib.h>

#include <platform.h>

#include "include/tegra194_ras.h"

/* Macro to indicate CPU to start an action */
#define START				U(0xAA55)

/* Global flag to indicate that a fault was received */
static volatile uint64_t fault_received;

/* SDEI handler to receive RAS UC errors */
extern int serror_sdei_event_handler(int ev, uint64_t arg);

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
	 * Per core RAS error records
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

static void test_ras_inject_serror(uint32_t errselr_el1, uint64_t pfg_ctlr)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1() & MPID_MASK);

	/*
	 * The per-cluster frequency monitoring nodes should be accessed from
	 * CPUs in the cluster that the node belongs to. e.g. nodes 0x200 and
	 * 0x201 should be accessed from CPUs in cluster 0, nodes 0x210 and
	 * 0x211 should be accessed from CPUs in cluster 1 and so on.
	 */
	if (((errselr_el1 & 0xF00) == 0x200) && ((errselr_el1 >> 4) & 0xF) != (core_pos >> 1)) {
		return;
	}

	/* clear the flag before we inject SError */
	fault_received = 0;
	dccvac((uint64_t)&fault_received);
	dmbish();

	INFO("mpidr=0x%lx, errselr_el1=0x%x, pfg_ctlr=0x%llx\n",
		read_mpidr_el1(), errselr_el1, pfg_ctlr);

	/* Choose error record */
	write_errselr_el1(errselr_el1);

	/* Program count down timer to 1 */
	write_erxpfgcdn_el1(1);

	/* Start count down to generate error on expiry */
	write_erxpfgctl_el1(ERXPFGCTL_UC_BIT | ERXPFGCTL_CDEN_BIT | pfg_ctlr);

	/* wait until the SError fires */
	do {
		dccivac((uint64_t)&fault_received);
		dmbish();
	} while (fault_received == 0);

	/*
	 * ACLR_EL1, Bit13 = RESET_RAS_FMON
	 *
	 * A write of 1 to this write-only bit re-enables checking for RAS
	 * frequency monitoring errors which are temporarily disabled when
	 * detected.
	 */
	if (((errselr_el1 & 0xF00) == 0x200) && ((errselr_el1 >> 4) & 0xF) == (core_pos >> 1))
		write_actlr_el1(read_actlr_el1() | BIT_32(13));
	else if ((errselr_el1 == 0x404))
		write_actlr_el1(read_actlr_el1() | BIT_32(13));
}

static void generate_uncorrectable_faults(void)
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

		for (j = 0; j < num_idx; j++) {
			uint32_t errselr_el1 = idx_start + j;
			uint64_t __unused err_fr;
			uint64_t uncorr_errs;

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
			 * Mask the uncorrectable errors that are disabled
			 * in the ERXFR register
			 */
			uncorr_errs = aux_data[j].err_ctrl();
			uncorr_errs &= err_fr;

			for (k = 32; k < 64; k++) {
				if (uncorr_errs & BIT_64(k)) {
					VERBOSE("ERR<x>CTLR bit%d\n", k);
					test_ras_inject_serror(errselr_el1, BIT_64(k));
				}
			}
		}
	}
}

int __unused sdei_handler(int ev, uint64_t arg)
{
	fault_received = 1;
	dccvac((uint64_t)&fault_received);
	dsbish();
	VERBOSE("SError SDEI event received.\n");
	return 0;
}

static event_t cpu_booted[PLATFORM_CORE_COUNT];
static volatile uint64_t cpu_powerdown[PLATFORM_CORE_COUNT];
static volatile uint64_t cpu_start_test[PLATFORM_CORE_COUNT];
static volatile uint64_t cpu_test_completed[PLATFORM_CORE_COUNT];

static void sdei_register_for_event(int event_id)
{
	int64_t ret = 0;

	/* Register SDEI handler */
	ret = sdei_event_register(event_id, serror_sdei_event_handler, 0,
			SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret < 0)
		tftf_testcase_printf("SDEI event register failed: 0x%llx\n",
			ret);

	ret = sdei_event_enable(event_id);
	if (ret < 0)
		tftf_testcase_printf("SDEI event enable failed: 0x%llx\n", ret);

	ret = sdei_pe_unmask();
	if (ret < 0)
		tftf_testcase_printf("SDEI pe unmask failed: 0x%llx\n", ret);
}

static test_result_t test_cpu_serrors(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	VERBOSE("Hello from core 0x%x\n", mpid);

	/* register for the SDEI event ID */
	sdei_register_for_event(300 + core_pos);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	/* Wait until lead CPU asks us to start the test */
	do {
		dccivac((uintptr_t)&cpu_start_test[core_pos]);
		dmbish();
	} while (!cpu_start_test[core_pos]);

	generate_uncorrectable_faults();

	VERBOSE("0x%lx: test complete\n", read_mpidr_el1());

	/* Inform lead CPU of test completion */
	cpu_test_completed[core_pos] = true;
	dccvac((uintptr_t)&cpu_test_completed[core_pos]);
	dsbish();

	/* Wait until lead CPU asks us to power down */
	do {
		dccivac((uintptr_t)&cpu_powerdown[core_pos]);
		dmbish();
	} while (!cpu_powerdown[core_pos]);

	return TEST_RESULT_SUCCESS;
}

test_result_t test_ras_uncorrectable(void)
{
	const int __unused event_id = 300;
	int64_t __unused ret = 0;
	unsigned int cpu_node, cpu_mpid;
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos;

	tftf_testcase_printf("Tegra194 uncorrectable RAS errors.\n");

	/* long execution test; reset watchdog */
	tftf_platform_watchdog_reset();

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {

		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t) test_cpu_serrors,
				0);
		if (ret != PSCI_E_SUCCESS)
			ret = TEST_RESULT_FAIL;
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

	/* register for the SDEI event ID */
	sdei_register_for_event(300);

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
		dccvac((uintptr_t)&cpu_start_test[core_pos]);
		dsbish();

		/* Wait for the CPU to complete the test */
		do {
			dccivac((uintptr_t)&cpu_test_completed[core_pos]);
			dmbish();
		} while (!cpu_test_completed[core_pos]);
	}

	/* run through all supported uncorrectable faults */
	generate_uncorrectable_faults();

	VERBOSE("0x%lx: test complete\n", read_mpidr_el1());

	/* Wait for all CPUs to power off */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/*
		 * Except lead CPU, Wait for all cores to be powered off
		 * by framework
		 */
		if (cpu_mpid == lead_mpid)
			continue;

		/* Allow other CPUs to start power down sequence */
		core_pos = platform_get_core_pos(cpu_mpid);
		cpu_powerdown[core_pos] = START;
		dccvac((uintptr_t)&cpu_powerdown[core_pos]);
		dsbish();

		/* Wait for the CPU to actually power off */
		while (tftf_psci_affinity_info(cpu_mpid, MPIDR_AFFLVL0) != PSCI_STATE_OFF)
			dsbsy();
	}

	return TEST_RESULT_SUCCESS;
}
