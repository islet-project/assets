/* Sample Linux PCI device driver */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/eventfd.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/file.h>
#include <linux/mm.h>

#define DEVICE_NAME "host_channel"
#define MINOR_BASE 0
#define MINOR_NUM 1

#define WRITE_DATA_SIZE (sizeof(int)*2) // should be matched with Eventfd Allocator Server

#if defined(CONFIG_INTER_REALM_SHM_SIZE_4KB)
#define INTER_REALM_SHM_SIZE (1 << 12)
#elif defined(CONFIG_INTER_REALM_SHM_SIZE_8KB)
#define INTER_REALM_SHM_SIZE (1 << 12) * 2
#elif defined(CONFIG_INTER_REALM_SHM_SIZE_16KB)
#define INTER_REALM_SHM_SIZE (1 << 12) * 4
#elif defined(CONFIG_INTER_REALM_SHM_SIZE_32KB)
#define INTER_REALM_SHM_SIZE (1 << 12) * 8 
#elif defined(CONFIG_INTER_REALM_SHM_SIZE_64KB)
#define INTER_REALM_SHM_SIZE (1 << 12) * 16
#endif

#define VMID_MAX 256

#define MMAP_OWNER_VMID_MASK 0xFF
#define MMAP_SHARE_OTHER_REALM_MEM_MASK 0x100

static int dev_major_num;

struct shared_realm_memory {
	struct list_head list;
	int vmid;
	int ref_cnt;
	u64 vm_start;
	u64 va;
	u64 phys;
	u64 ipa;
};

struct channel_priv { // This is a "private" data structure
	struct {
		spinlock_t lock;
		struct list_head heads[VMID_MAX];
	} shrms;
	bool is_active; // TODO: do we need this ?
};

static struct channel_priv drv_priv;
static struct cdev ch_cdev;
static struct class *ch_class;

static void mmap_vma_close(struct vm_area_struct *vma)
{
	u64 offset = vma->vm_pgoff, ipa = offset & PAGE_MASK;
	int vmid = offset & MMAP_OWNER_VMID_MASK;
	struct shared_realm_memory *tmp = NULL;

	pr_info("%s: start. ipa 0x%llx, vmid %d", __func__, ipa, vmid);

	spin_lock_irq(&drv_priv.shrms.lock);
	list_for_each_entry(tmp, &drv_priv.shrms.heads[vmid], list) {
		if (ipa == tmp->ipa) {
			pr_info("%s: ipa 0x%llx, vmid %d ref_cnt %d", __func__, ipa, vmid, tmp->ref_cnt);
			if (--tmp->ref_cnt == 0) {
				list_del(&tmp->list);
				free_pages(tmp->va, get_order(INTER_REALM_SHM_SIZE));
				kfree(tmp);
				pr_info("%s: shrm with ipa 0x%llx is freed", __func__, ipa);
			}
			break;
		}
	}
	spin_unlock_irq(&drv_priv.shrms.lock);
	pr_info("%s: end. ipa 0x%llx, vmid %d", __func__, ipa, vmid);
}

static struct vm_operations_struct channel_vm_ops = {
	.close = mmap_vma_close,
};

static void ch_deactivate(struct channel_priv *priv_ptr)
{
	BUG_ON(!priv_ptr->is_active);
	priv_ptr->is_active = false;
}

static int channel_open(struct inode *inode, struct file *file)
{
	pr_info("[CH] device opened\n");
    return 0;
}

static int channel_release(struct inode * inode, struct file * file)
{
    pr_info("[CH] channel_release start");
    if (file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

static int channel_mmap(struct file *filp, struct vm_area_struct *vma) 
{
	u64 req_size = vma->vm_end - vma->vm_start;
	u64 offset = vma->vm_pgoff, phys = 0, ipa = offset & PAGE_MASK;
	int vmid = offset & MMAP_OWNER_VMID_MASK;

	pr_info("[HCH] mmap vm_flags 0x%llx vm_page_prot 0x%llx offset 0x%llx\n",
			vma->vm_flags, vma->vm_page_prot, offset);

	if (req_size != INTER_REALM_SHM_SIZE) {
		pr_err("%s Incorrect req_size 0x%llx != 0x%llx\n", __func__, req_size, INTER_REALM_SHM_SIZE);
		return -EINVAL;
	}

	if (offset & MMAP_SHARE_OTHER_REALM_MEM_MASK) {
		struct shared_realm_memory *tmp, *target = NULL;

		spin_lock_irq(&drv_priv.shrms.lock);
		list_for_each_entry(tmp, &drv_priv.shrms.heads[vmid], list) {
			if (ipa == tmp->ipa) {
				if (tmp->ref_cnt >= 2) {
					pr_err("%s ref_cnt shouldn't be greater than 2 but %d", __func__, tmp->ref_cnt);
					spin_unlock_irq(&drv_priv.shrms.lock);
					return -EINVAL;
				} else if (!tmp->phys) {
					pr_err("%s there is no matched shrm with the ipa 0x%llx", __func__, ipa);
					spin_unlock_irq(&drv_priv.shrms.lock);
					return -EINVAL;
				}
				target = tmp;
				target->ref_cnt++;
				phys = target->phys;
				break;
			}
		}
		spin_unlock_irq(&drv_priv.shrms.lock);

		if (!target) {
			pr_err("[HCH] %s there is no shrm with the ipa 0x%llx", __func__, ipa);
			return -EINVAL;
		}

		pr_info("[HCH] %s founded the target shrm with ipa 0x%llx", __func__, ipa);
	} else {
		struct shared_realm_memory *shrm;
		void *va = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO | __GFP_MOVABLE,
				get_order(INTER_REALM_SHM_SIZE));
		if (!va) {
			pr_err("%s __get_free_pages failed\n", __func__);
			return -ENOMEM;
		}

		shrm = kzalloc(sizeof(*shrm), GFP_KERNEL_ACCOUNT);
		if (!shrm) 
			return -ENOMEM;

		INIT_LIST_HEAD(&shrm->list);
		shrm->ipa = ipa;
		shrm->vmid = vmid;
		shrm->ref_cnt = 1;
		shrm->va = va;
		shrm->phys = __pa(va);
		phys = shrm->phys;

		spin_lock_irq(&drv_priv.shrms.lock);
		list_add_tail(&shrm->list, &drv_priv.shrms.heads[shrm->vmid]);
		spin_unlock_irq(&drv_priv.shrms.lock);

		pr_info("[HCH] mmap va %p, pa %llx, size 0x%llx, shm_owner_vmid %d \n",
				va, shrm->phys, INTER_REALM_SHM_SIZE, shrm->vmid);
	}

	vma->vm_ops = &channel_vm_ops;

	/* Mapping pages to user process */
	return remap_pfn_range(vma, vma->vm_start,
			       PFN_DOWN(phys), INTER_REALM_SHM_SIZE, vma->vm_page_prot);
}

static const struct file_operations ch_fops = {
	.owner   = THIS_MODULE,
	.open	= channel_open,
	.release = channel_release,
	.mmap = channel_mmap,
};

static int __init channel_init(void)
{
	int ret = 0;
	struct device *dev_struct;
	dev_t ch_dev;

	pr_info("[CH] channel_init start \n");

	ret = alloc_chrdev_region(&ch_dev, MINOR_BASE, MINOR_NUM, DEVICE_NAME);
	if (ret) {
		pr_err("[CH] %s alloc_chrdev_region failed %d\n", __func__, ret);
		return ret;
	}

	cdev_init(&ch_cdev, &ch_fops);

	ret = cdev_add(&ch_cdev, ch_dev, MINOR_NUM);
	if (ret) {
		pr_err("[CH] %s cdev_add failed %d\n", __func__, ret);
		goto unreg_dev_num;
	}

	ch_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(ch_class)) {
		pr_err("[CH] %s class_create failed\n", __func__);
		goto unreg_cdev;
	}

	dev_struct = device_create(ch_class, NULL, ch_dev, NULL, DEVICE_NAME);
	if (IS_ERR(dev_struct)) {
		pr_err("[CH] %s device_create failed\n", __func__);
		goto unreg_class;
	}

	memset(&drv_priv, 0, sizeof(drv_priv));
	spin_lock_init(&drv_priv.shrms.lock);
	drv_priv.is_active = true;

	spin_lock_irq(&drv_priv.shrms.lock);
	for(int i = 0 ; i < VMID_MAX; i++) {
		INIT_LIST_HEAD(&drv_priv.shrms.heads[i]);
	}
	spin_unlock_irq(&drv_priv.shrms.lock);

	dev_major_num = MAJOR(ch_dev);
	pr_info("[CH] %s major:minor = %d:%d\n", __func__, dev_major_num, MINOR(ch_dev));

	return 0;

unreg_class:
	class_destroy(ch_class);
unreg_cdev:
	cdev_del(&ch_cdev);
unreg_dev_num:
	unregister_chrdev_region(ch_dev, MINOR_NUM);
	return ret ? ret : -1;
}

static void __exit channel_exit(void)
{
	dev_t dev = MKDEV(dev_major_num, MINOR_BASE);

	// this form is only valid when MINOR_NUM is 1.
	device_destroy(ch_class, dev);
	class_destroy(ch_class);
	cdev_del(&ch_cdev);
	unregister_chrdev_region(dev, MINOR_NUM);
	ch_deactivate(&drv_priv);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunwook Eom <speed.eom@samsung.com>");
MODULE_VERSION("0.1");

module_init(channel_init);
module_exit(channel_exit);
