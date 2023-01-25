/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Arm Limited.
 * All rights reserved.
 */

#include <asm/io.h>
#include <alloc_page.h>
#include <bitops.h>

#define GRANULE_SIZE 	0x1000
#define BUF_SIZE	(PAGE_SIZE * 2)
#define BUF_PAGES	(BUF_SIZE / PAGE_SIZE)
#define BUF_GRANULES	(BUF_SIZE / GRANULE_SIZE)

static char __attribute__((aligned(PAGE_SIZE))) buffer[BUF_SIZE];

static void static_shared_buffer_test(void)
{
	int i;

	set_memory_decrypted((unsigned long)buffer, sizeof(buffer));
	for (i = 0; i < sizeof(buffer); i += GRANULE_SIZE)
		buffer[i] = (char)i;

	/*
	 * Verify the content of the NS buffer
	 */
	for (i = 0; i < sizeof(buffer); i += GRANULE_SIZE) {
		if (buffer[i] != (char)i) {
			report(false, "Failed to set Non Secure memory");
			return;
		}
	}

	/* Make the buffer back to protected... */
	set_memory_encrypted((unsigned long)buffer, sizeof(buffer));
	/* .. and check if the contents were destroyed */
	for (i = 0; i < sizeof(buffer); i += GRANULE_SIZE) {
		if (buffer[i] != 0) {
			report(false, "Failed to scrub protected memory");
			return;
		}
	}

	report(true, "Conversion of protected memory to shared and back");
}

static void dynamic_shared_buffer_test(void)
{
	char *ns_buffer;
	int i;
	int order = get_order(BUF_PAGES);

	ns_buffer = alloc_pages_shared(order);
	assert(ns_buffer);
	for (i = 0; i < sizeof(buffer); i += GRANULE_SIZE)
		ns_buffer[i] = (char)i;

	/*
	 * Verify the content of the NS buffer
	 */
	for (i = 0; i < sizeof(buffer); i += GRANULE_SIZE) {
		if (ns_buffer[i] != (char)i) {
			report(false, "Failed to set Non Secure memory");
			return;
		}
	}
	free_pages_shared(ns_buffer);
	report(true, "Dynamic allocation and free of shared memory\n");
}

static void ns_test(void)
{
	static_shared_buffer_test();
	dynamic_shared_buffer_test();
}

int main(int argc, char **argv)
{
	report_prefix_pushf("ns-memory");
	ns_test();
	report_prefix_pop();

	return report_summary();
}
