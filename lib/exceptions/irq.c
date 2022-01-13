/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <sgi.h>
#include <spinlock.h>
#include <string.h>
#include <tftf.h>
#include <tftf_lib.h>

#define IS_PLAT_SPI(irq_num)						\
	(((irq_num) >= MIN_SPI_ID) &&					\
	 ((irq_num) <= MIN_SPI_ID + PLAT_MAX_SPI_OFFSET_ID))

static spi_desc spi_desc_table[PLAT_MAX_SPI_OFFSET_ID + 1];
static ppi_desc ppi_desc_table[PLATFORM_CORE_COUNT][
				(MAX_PPI_ID + 1) - MIN_PPI_ID];
static sgi_desc sgi_desc_table[PLATFORM_CORE_COUNT][MAX_SGI_ID + 1];
static spurious_desc spurious_desc_handler;

/*
 * For a given SPI, the associated IRQ handler is common to all CPUs.
 * Therefore, we need a lock to prevent simultaneous updates.
 *
 * We use one lock for all SPIs. This will make it impossible to update
 * different SPIs' handlers at the same time (although it would be fine) but it
 * saves memory. Updating an SPI handler shouldn't occur that often anyway so we
 * shouldn't suffer from this restriction too much.
 */
static spinlock_t spi_lock;

static irq_handler_t *get_irq_handler(unsigned int irq_num)
{
	if (IS_PLAT_SPI(irq_num))
		return &spi_desc_table[irq_num - MIN_SPI_ID].handler;

	unsigned int mpid = read_mpidr_el1();
	unsigned int linear_id = platform_get_core_pos(mpid);

	if (IS_PPI(irq_num))
		return &ppi_desc_table[linear_id][irq_num - MIN_PPI_ID].handler;

	if (IS_SGI(irq_num))
		return &sgi_desc_table[linear_id][irq_num - MIN_SGI_ID].handler;

	/*
	 * The only possibility is for it to be a spurious
	 * interrupt.
	 */
	assert(irq_num == GIC_SPURIOUS_INTERRUPT);
	return &spurious_desc_handler;
}

void tftf_send_sgi(unsigned int sgi_id, unsigned int core_pos)
{
	assert(IS_SGI(sgi_id));

	/*
	 * Ensure that all memory accesses prior to sending the SGI are
	 * completed.
	 */
	dsbish();

	/*
	 * Don't send interrupts to CPUs that are powering down. That would be a
	 * violation of the PSCI CPU_OFF caller responsibilities. The PSCI
	 * specification explicitely says:
	 * "Asynchronous wake-ups on a core that has been switched off through a
	 * PSCI CPU_OFF call results in an erroneous state. When this erroneous
	 * state is observed, it is IMPLEMENTATION DEFINED how the PSCI
	 * implementation reacts."
	 */
	assert(tftf_is_core_pos_online(core_pos));
	arm_gic_send_sgi(sgi_id, core_pos);
}

void tftf_irq_enable(unsigned int irq_num, uint8_t irq_priority)
{
	if (IS_PLAT_SPI(irq_num)) {
		/*
		 * Instruct the GIC Distributor to forward the interrupt to
		 * the calling core
		 */
		arm_gic_set_intr_target(irq_num, platform_get_core_pos(read_mpidr_el1()));
	}

	arm_gic_set_intr_priority(irq_num, irq_priority);
	arm_gic_intr_enable(irq_num);

	VERBOSE("Enabled IRQ #%u\n", irq_num);
}

void tftf_irq_disable(unsigned int irq_num)
{
	/* Disable the interrupt */
	arm_gic_intr_disable(irq_num);

	VERBOSE("Disabled IRQ #%u\n", irq_num);
}

#define HANDLER_VALID(handler, expect_handler)		\
	((expect_handler) ? ((handler) != NULL) : ((handler) == NULL))

static int tftf_irq_update_handler(unsigned int irq_num,
				   irq_handler_t irq_handler,
				   bool expect_handler)
{
	irq_handler_t *cur_handler;
	int ret = -1;

	cur_handler = get_irq_handler(irq_num);
	if (IS_PLAT_SPI(irq_num))
		spin_lock(&spi_lock);

	/*
	 * Update the IRQ handler, if the current handler is in the expected
	 * state
	 */
	assert(HANDLER_VALID(*cur_handler, expect_handler));
	if (HANDLER_VALID(*cur_handler, expect_handler)) {
		*cur_handler = irq_handler;
		ret = 0;
	}

	if (IS_PLAT_SPI(irq_num))
		spin_unlock(&spi_lock);

	return ret;
}

int tftf_irq_register_handler(unsigned int irq_num, irq_handler_t irq_handler)
{
	int ret;

	ret = tftf_irq_update_handler(irq_num, irq_handler, false);
	if (ret == 0)
		INFO("Registered IRQ handler %p for IRQ #%u\n",
			(void *)(uintptr_t) irq_handler, irq_num);

	return ret;
}

int tftf_irq_unregister_handler(unsigned int irq_num)
{
	int ret;

	ret = tftf_irq_update_handler(irq_num, NULL, true);
	if (ret == 0)
		INFO("Unregistered IRQ handler for IRQ #%u\n", irq_num);

	return ret;
}

int tftf_irq_handler_dispatcher(void)
{
	unsigned int raw_iar;
	unsigned int irq_num;
	sgi_data_t sgi_data;
	irq_handler_t *handler;
	void *irq_data = NULL;
	int rc = 0;

	/* Acknowledge the interrupt */
	irq_num = arm_gic_intr_ack(&raw_iar);

	handler = get_irq_handler(irq_num);
	if (IS_PLAT_SPI(irq_num)) {
		irq_data = &irq_num;
	} else if (IS_PPI(irq_num)) {
		irq_data = &irq_num;
	} else if (IS_SGI(irq_num)) {
		sgi_data.irq_id = irq_num;
		irq_data = &sgi_data;
	}

	if (*handler != NULL)
		rc = (*handler)(irq_data);

	/* Mark the processing of the interrupt as complete */
	if (irq_num != GIC_SPURIOUS_INTERRUPT)
		arm_gic_end_of_intr(raw_iar);

	return rc;
}

void tftf_irq_setup(void)
{
	memset(spi_desc_table, 0, sizeof(spi_desc_table));
	memset(ppi_desc_table, 0, sizeof(ppi_desc_table));
	memset(sgi_desc_table, 0, sizeof(sgi_desc_table));
	memset(&spurious_desc_handler, 0, sizeof(spurious_desc_handler));
	init_spinlock(&spi_lock);
}
