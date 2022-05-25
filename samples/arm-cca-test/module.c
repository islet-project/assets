// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 ARM Ltd, All Rights Reserved.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <asm/rmi_cmds.h>

static struct {
	struct dentry *debugfs;		/* debugfs directory node */
	unsigned long page;		/* Page reserved for use by tests */
	phys_addr_t paddr;		/* Page physical address */
} arm_cca_test_module;

static int realm_granule_show(struct seq_file *seq, void *data)
{
	unsigned long long val;

	val = READ_ONCE(*(u64 *)(arm_cca_test_module.page));

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(realm_granule);

static void arm_cca_test_module_exit(void)
{
	debugfs_remove_recursive(arm_cca_test_module.debugfs);

	rmi_granule_undelegate(arm_cca_test_module.paddr);

	if (arm_cca_test_module.page)
		free_page(arm_cca_test_module.page);

	memset(&arm_cca_test_module, 0, sizeof(arm_cca_test_module));
}
module_exit(arm_cca_test_module_exit);

static int arm_cca_test_module_init(void)
{
	struct dentry *debugfs;
	unsigned long ret;
	unsigned long page;
	phys_addr_t paddr;

	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	paddr = virt_to_phys((void *)page);
	ret = rmi_granule_delegate(paddr);
	if (ret) {
		free_page(arm_cca_test_module.page);
		if (ret == SMCCC_RET_NOT_SUPPORTED)
			return -ENXIO;

		pr_err("Granule delegate Failed! ret 0x%lx\n", ret);
		return ret;
	}

	debugfs = debugfs_create_dir("arm_cca_test", NULL);
	if (IS_ERR(debugfs))
		goto out_err;

	arm_cca_test_module.debugfs = debugfs;
	debugfs = debugfs_create_file("realm_granule_access", 0444,
				      arm_cca_test_module.debugfs, NULL,
				      &realm_granule_fops);
	if (IS_ERR(debugfs))
		goto out_err;

	arm_cca_test_module.page = page;
	arm_cca_test_module.paddr = paddr;

	return 0;

out_err:
	arm_cca_test_module_exit();
	return PTR_ERR(debugfs);
}
module_init(arm_cca_test_module_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Testmore test suite");
