#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/rsi_cmds.h>
#include <linux/workqueue.h>
#include <linux/io.h>

#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/file.h>
#include <linux/mm.h>

#include "dyn_shrm_manager.h"

/* for the character device */
#define DEVICE_NAME "gch_char"
#define MINOR_BASE 0
#define MINOR_NUM 1

static int dev_major_num;
static struct cdev ch_cdev;
static struct class *ch_class;

/* for the PCI device */
#define DRIVER_NAME "guest_channel"
#define VENDOR_ID 0x1af4
#define DEVICE_ID 0x1110 // temporarily uses ivshmem's device id
#define MINOR_BASE 0

#define IOEVENTFD_BASE_ADDR 0x7fffff00
#define IOEVENTFD_BASE_SIZE 0x100

#define SHRM_BASE_IPA     0xC0000000
#define MAX_SHRM_IPA_SIZE 0x10000000

#define BAR_MMIO_CURRENT_VMID 0
#define BAR_MMIO_PEER_VMID 4 
#define BAR_MMIO_SHM_RW_IPA_BASE 8
#define BAR_MMIO_SHM_RO_IPA_BASE 12
#define BAR_MMIO_UNMAP_SHRM_IPA  16

#define BAR_MMIO_MIN_OFFSET BAR_MMIO_CURRENT_VMID
#define BAR_MMIO_MAX_OFFSET BAR_MMIO_SHM_RO_IPA_BASE

#define INVALID_PEER_ID -1

struct channel_priv *drv_priv;
u64 test_shrm_offset = 0x0;

/* This sample driver supports device with VID = 0x010F, and PID = 0x0F0E*/
static struct pci_device_id channel_id_table[] = {
    { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
    {0,}
};

MODULE_DEVICE_TABLE(pci, channel_id_table);

static int channel_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void channel_remove(struct pci_dev *pdev);

/* Driver registration structure */
static struct pci_driver channel_driver = {
    .name = DRIVER_NAME,
    .id_table = channel_id_table,
    .probe = channel_probe,
    .remove = channel_remove
};

typedef enum {
	SHM_ALLOCATOR = 0, // It's host module
	SERVER = 1,
	CLIENT = 2,
} ROLE;

struct peer {
    int id;      /* This is for identifying peers. It's NOT vmid */
    int eventfd;
};

/* This is a "private" data structure */
/* You can store there any data that should be passed between driver's functions */
struct channel_priv {
	uint32_t* ioeventfd_addr;
	int vmid;
	struct peer peer;
	ROLE role;
	struct work_struct send, receive;
	struct work_struct rt_sender, rt_receiver; // for the dyn_shrm_remove_test
	u64* mapped_bar_addr;
	u64 *rw_shrm_va_start;
	u64 *ro_shrm_va_start;
};

static u64 get_ipa_start(int vmid) {
	if (vmid <= 0) {
		pr_err("%s vmid should be greater than 0 but %d", __func__, vmid);
		return 0;
	}
	return SHRM_BASE_IPA + (vmid-1) * MAX_SHRM_IPA_SIZE;
}

u64* get_shrm_va(bool read_only, u64 ipa) {
	u64 shrm_va = (read_only) ? (u64)drv_priv->ro_shrm_va_start : (u64)drv_priv->rw_shrm_va_start;

	if (!shrm_va) {
		pr_err("%s shrm_va shouldn't be non-zero. read_only %d. ipa 0x%llx",
				__func__, read_only, ipa);
		return NULL;
	}

	shrm_va += ipa % MAX_SHRM_IPA_SIZE;
	return (u64*)shrm_va;
}

void send_signal(int peer_id) {
	pr_info("[GCH] write %d to ioeventfd_addr 0x%x", peer_id, (uint32_t)drv_priv->ioeventfd_addr);
	iowrite32(peer_id, drv_priv->ioeventfd_addr);
}

static void get_cur_vmid(void) {
	int vmid;

	if (drv_priv->vmid) {
		pr_info("[GCH] %s vmid is already set %d",__func__, drv_priv->vmid);
		return;
	}
	
	vmid = readl(drv_priv->mapped_bar_addr + BAR_MMIO_CURRENT_VMID);

	if (vmid == INVALID_PEER_ID || vmid == SHM_ALLOCATOR) {
		pr_info("[GCH] The vmid is not valid %d", vmid);
	} else {
		pr_info("[GCH] get vmid %d", vmid);
		drv_priv->vmid = vmid;
		drv_priv->role = (vmid == CLIENT) ? CLIENT : SERVER;
		pr_info("[GCH] my role is %d", drv_priv->role);
	}
}

static void set_peer_id(void) {
	int peer_id;

	if (drv_priv->peer.id) {
		pr_info("[GCH] %s peer id is already set %d",__func__, drv_priv->peer.id);
		return;
	}
	
	peer_id = readl(drv_priv->mapped_bar_addr + BAR_MMIO_PEER_VMID);

	if (peer_id == INVALID_PEER_ID || peer_id == SHM_ALLOCATOR) {
		pr_info("[GCH] peer_id is not valid %d", peer_id);
	} else {
		pr_info("[GCH] get peer_id %d", peer_id);
		drv_priv->peer.id = peer_id;
	}
}

static s64 mmio_read(u64 offset) {
	s64 ret;

	switch(offset) {
		case BAR_MMIO_CURRENT_VMID:
		case BAR_MMIO_PEER_VMID:
		case BAR_MMIO_SHM_RW_IPA_BASE:
		case BAR_MMIO_SHM_RO_IPA_BASE:
			ret = readl(drv_priv->mapped_bar_addr + offset);
			return ret;
		default:
			pr_err("%s wrong mmio offset 0x%llx", __func__, offset);
			return -EINVAL;
	}
}

static int mmio_write(u64 offset, u64 val) {
	int ret = 0;

	switch(offset) {
		case BAR_MMIO_CURRENT_VMID:
		case BAR_MMIO_PEER_VMID:
		case BAR_MMIO_SHM_RW_IPA_BASE:
		case BAR_MMIO_SHM_RO_IPA_BASE:
		case BAR_MMIO_UNMAP_SHRM_IPA:
			writel(val, drv_priv->mapped_bar_addr + offset);
			return ret;
		default:
			pr_err("%s wrong mmio offset 0x%llx with val 0x%llx", __func__, offset, val);
			return -EINVAL;
	}
}

s64 mmio_read_to_get_shrm(void) {
	return mmio_read(BAR_MMIO_SHM_RW_IPA_BASE);
}

int mmio_write_to_remove_shrm(u64 ipa) {
	return mmio_write(BAR_MMIO_SHM_RW_IPA_BASE, ipa);
}

int mmio_write_to_unmap_shrm(u64 ipa) {
	return mmio_write(BAR_MMIO_UNMAP_SHRM_IPA, ipa);
}

static void ch_send(struct work_struct *work) {
	int ret;
	u64 msg = 0xBEEF;
	u64 *rw_shrm_va = get_shrm_va(false, test_shrm_offset);
	//u64 *rw_shrm_va = drv_priv->rw_shrm_va_start;

	set_peer_id();

	pr_err("[GCH] %s start. And my role: %d", __func__, drv_priv->role);
	if (drv_priv->role != CLIENT) {
		pr_err("[GCH] My role is not CLIENT but %d", drv_priv->role);
		return;
	}
	write_to_shrm(NULL, NULL, 0); // for the build test

	if (!rw_shrm_va) {
		pr_err("[GCH] %s rw_shrm_va shouldn't be NULL", __func__);
		return;
	}

	pr_info("[GCH] %s drv_priv->rw_shrm_va_start 0x%llx, rw_shrm_va: 0x%llx",
			__func__, drv_priv->rw_shrm_va_start, rw_shrm_va);

	/*
	ret = req_shrm_chunk();
	if (ret) { //for the test
		pr_err("[GCH] %s req_shrm_chunk() is failed with %d\n", __func__, ret);
		return;
	}
	*/

	pr_err("[GCH] %s before writing msg to rw_shrm_va: 0x%llx\n", __func__, *rw_shrm_va);
	*rw_shrm_va = msg;
	pr_err("[GCH] %s after writing msg to rw_shrm_va: 0x%llx\n", __func__, *rw_shrm_va);
	send_signal(drv_priv->peer.id);
	pr_info("[GCH] %s done");
}

static void ch_receive(struct work_struct *work) {
	u64 shrm_ipa, ipa_offset = test_shrm_offset, msg = 0;

	set_peer_id();

	pr_err("[GCH] %s start. And my role: %d", __func__, drv_priv->role);
	if (drv_priv->role != SERVER) {
		pr_err("[GCH] My role is not SERVER but %d", drv_priv->role);
		return;
	}
	copy_from_shrm(NULL, NULL); // for the build test
	shrm_ipa = get_ipa_start(drv_priv->peer.id);

	if (!drv_priv->ro_shrm_va_start) {
		u64 shrm_ro_ipa_start = shrm_ipa;
		shrm_ipa += ipa_offset;

		drv_priv->ro_shrm_va_start = memremap(shrm_ro_ipa_start, MAX_SHRM_IPA_SIZE, MEMREMAP_WB);
		if (!drv_priv->ro_shrm_va_start) {
			pr_err("%s memremap for 0x%llx failed \n", __func__, shrm_ro_ipa_start);
			return;
		}
		pr_info("[GCH] %s memremap result: 0x%llx for [0x%llx:0x%llx)", __func__,
				drv_priv->ro_shrm_va_start, shrm_ro_ipa_start, shrm_ro_ipa_start + MAX_SHRM_IPA_SIZE);

		mmio_write(BAR_MMIO_SHM_RO_IPA_BASE, shrm_ipa);

		pr_info("[GCH] %s call set_memory_shared with shrm_ipa 0x%llx va: %p",
				__func__, shrm_ipa, drv_priv->ro_shrm_va_start);
		//set_memory_shared(shrm_ipa, 1); // don't need it. cos the mapping is already done using mmap write trap
	}

	if (drv_priv->ro_shrm_va_start)  {
		u64 *ro_shrm_va = get_shrm_va(true, ipa_offset);
		pr_err("[GCH] %s let's read & write the ro_shrm_va 0x%llx to msg", __func__, ro_shrm_va);

		msg = *ro_shrm_va;

		/*
		for (int i; i < 4; i++) {
			char* ptr = (char*)drv_priv->ro_shrm_va_start;
			pr_err("ro_shrm_va_start[%d]: %c\n", i, ptr[i]);
		}

		pr_err("[GCH] strncpy start from ro_shrm_va_start: %llx to msg\n", drv_priv->ro_shrm_va_start);
		strncpy(msg, (const char *)drv_priv->ro_shrm_va_start, sizeof(msg));
		*/
		pr_err("[GCH] %s msg read result: 0x%llx\n", __func__, msg);
	} else {
		pr_err("[GCH] drv_priv->ro_shrm_va_start is zero.\n");
	}
}


static void dyn_rt_sender(struct work_struct *work) {
	u64 client_shrm_ipa = get_ipa_start(CLIENT) + test_shrm_offset;

	pr_info("%s client_shrm_ipa: 0x%llx", __func__, client_shrm_ipa);

	remove_shrm_chunk(client_shrm_ipa);
	pr_info("%s done", __func__);
}

static void dyn_rt_receiver(struct work_struct *work) {
	u64 client_shrm_ipa = get_ipa_start(CLIENT) + test_shrm_offset;

	pr_info("%s mmio_write_to_unmap_shrm start", __func__);
	mmio_write_to_unmap_shrm(client_shrm_ipa);
	pr_info("%s mmio_write_to_unmap_shrm end", __func__);
}

static int channel_open(struct inode *inode, struct file *file)
{
	pr_info("[GCH] %s start", __func__);
    return 0;
}

static int channel_release(struct inode * inode, struct file * file)
{
	pr_info("[GCH] %s start", __func__);
    if (file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

static ssize_t channel_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	pr_info("[GCH] %s start", __func__);

	if (drv_priv->role == CLIENT) {
		pr_info("[GCH] %s start schedule_work for send", __func__);
		schedule_work(&drv_priv->send);
	} else if (drv_priv->role == SERVER) {
		pr_info("[GCH] %s start schedule_work for receive", __func__);
		schedule_work(&drv_priv->receive);
	} else {
		pr_info("[GCH] %s set_peer_id start", __func__);
		set_peer_id();
	}
    return 0;
}

static ssize_t channel_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	u64 buffer[10] = {};
	u64 client_shrm_ipa = get_ipa_start(CLIENT) + test_shrm_offset;

	pr_info("%s client_shrm_ipa: 0x%llx", __func__, client_shrm_ipa);
    if (copy_from_user(&buffer, buf, count) != 0) {
        return -EFAULT;
    }

	switch(drv_priv->role) {
		case CLIENT:
			pr_info("[GCH] %s start schedule_work for rt_sender", __func__);
			schedule_work(&drv_priv->rt_sender);
			break;
		case SERVER:
			pr_info("[GCH] %s start schedule_work for rt_receiver", __func__);
			schedule_work(&drv_priv->rt_receiver);
			break;
		default:
			pr_err("%s role %d is invalid", __func__, drv_priv->role);
	}

	pr_info("%s done with 0x%llx", __func__, buffer[0]);

    return count;
}

static const struct file_operations ch_fops = {
	.owner   = THIS_MODULE,
	.open	= channel_open,
	.release = channel_release,
	.read = channel_read,
	.write = channel_write,
};

static int __init channel_init(void)
{
	int ret = 0;
	struct device *dev_struct;
	dev_t ch_dev;

	pr_info("[GCH] %s start\n", __func__);

	/* setup character device */
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
	dev_major_num = MAJOR(ch_dev);
	pr_info("[CH] %s major:minor = %d:%d\n", __func__, dev_major_num, MINOR(ch_dev));

	/* setup pci device */
	ret = pci_register_driver(&channel_driver);
	if (ret) {
		pr_err("[GCH] pci_register_driver failed %d\n", ret);
	}

	return ret;

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

	pr_info("[GCH] %s start\n", __func__);

	device_destroy(ch_class, dev);
	class_destroy(ch_class);
	cdev_del(&ch_cdev);
	unregister_chrdev_region(dev, MINOR_NUM);

    pci_unregister_driver(&channel_driver);
}



/* 
 * There are two differnt logic based by caller layer (Host or Realm)
 * From Host: Shared memory is arrived 
 * From Realm: Receive I/O ring request 
 */
static irqreturn_t channel_irq_handler(int irq, void* dev_instance)
{
	//struct peer* peer = (struct peer*)dev_instance;
	static int cnt = 0;
	pr_info("[GCH] IRQ #%d cnt %d\n", irq, cnt);

	if (drv_priv->role == SERVER) {
		pr_info("[GCH] %s start schedule_work for receive", __func__);
		schedule_work(&drv_priv->receive);
	}

	return (cnt++) ? IRQ_NONE : IRQ_HANDLED;
	//return IRQ_HANDLED;
}

/* This function is called by the kernel */
static int channel_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int bar = 0, ret = -EBUSY;
    u16 vendor, device;
    uint32_t dev_ioeventfd_addr, dev_ioeventfd_size;
    uint32_t bar_addr, bar_size;
	u64 shrm_rw_ipa_start;
	s64 shrm_ipa;

    pr_info("[GCH] %s start\n", __func__);

    /* Let's read data from the PCI device configuration registers */
    pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
    pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

    pr_info("[GCH] device vid: 0x%X pid: 0x%X\n", vendor, device);

    ret = pci_enable_device(pdev);
    if (ret) {
		pr_err("[GCH] pci_enable_device failed %d\n", ret);
        return ret;
    }

    /* Allocate memory for the driver private data */
    drv_priv = kzalloc(sizeof(struct channel_priv), GFP_KERNEL);
    if (!drv_priv) {
		pr_err("[GCH] failed to kzalloc for drv_priv\n");
		goto pci_disable;
    }

	drv_priv->role = -1;

	pr_info("[GCH] %s init_work for send, receive start\n", __func__);
	INIT_WORK(&drv_priv->send, ch_send);
	INIT_WORK(&drv_priv->receive, ch_receive);
	INIT_WORK(&drv_priv->rt_sender, dyn_rt_sender);
	INIT_WORK(&drv_priv->rt_receiver, dyn_rt_receiver);

	/* Request memory region for the BAR */
	ret = pci_request_region(pdev, bar, DRIVER_NAME);
	if (ret) {
		pr_err("[GCH] pci_request_region failed %d\n", ret);
		goto pci_disable;
	}

	bar_addr = pci_resource_start(pdev, bar);
	bar_size = pci_resource_len(pdev, bar);

	drv_priv->mapped_bar_addr = pci_iomap(pdev, bar, 0);
	if (!drv_priv->mapped_bar_addr) {
		pr_info("[GCH] pci_iomap is failed %llx", (u64)drv_priv->mapped_bar_addr);
		ret = -ENOMEM;
		goto pci_disable;
	}

	pr_info("[GCH] %s call get_cur_vmid", __func__);
	get_cur_vmid();

	pr_info("[GCH] %s call get_ipa_start", __func__);
	shrm_rw_ipa_start = get_ipa_start(drv_priv->vmid);
	if (!shrm_rw_ipa_start) {
		ret = -EINVAL;
		goto pci_disable;
	}
	//shrm_rw_ipa_start += 0x1000;

	ret = init_shrm_list(shrm_rw_ipa_start, MAX_SHRM_IPA_SIZE);
	if (ret) {
		goto pci_disable;
	}

	pr_info("[GCH] %s call memremap to map 0x%llx", __func__, shrm_rw_ipa_start);
	//drv_priv->rw_shrm_va_start = memremap(shrm_rw_ipa_start, 0x1000, MEMREMAP_WB);
	drv_priv->rw_shrm_va_start = memremap(shrm_rw_ipa_start, MAX_SHRM_IPA_SIZE, MEMREMAP_WB);
	if (!drv_priv->rw_shrm_va_start) {
		pr_err("%s memremap for 0x%llx failed \n", __func__, shrm_rw_ipa_start);
		ret = -ENOMEM;
		goto pci_disable;
	}

	pr_info("[GCH] %s memremap result: 0x%llx for [0x%llx:0x%llx)", __func__,
			drv_priv->rw_shrm_va_start, shrm_rw_ipa_start, shrm_rw_ipa_start + MAX_SHRM_IPA_SIZE);

	set_peer_id();

	dev_ioeventfd_addr = IOEVENTFD_BASE_ADDR;
	dev_ioeventfd_size = IOEVENTFD_BASE_SIZE;
	drv_priv->ioeventfd_addr = ioremap(dev_ioeventfd_addr, dev_ioeventfd_size);

	pr_info("[GCH] ioeventfd addr 0x%x, size 0x%x, iomap_addr 0x%llx",
			dev_ioeventfd_addr, dev_ioeventfd_size, (uint64_t)drv_priv->ioeventfd_addr);
	if (!drv_priv->ioeventfd_addr) {
		pr_err("[GCH] pci_iomap failed for ioeventfd_addr\n");
		goto pci_disable;
	}

    /* Set driver private data */
    /* Now we can access mapped "hwmem" from the any driver's function */
    pci_set_drvdata(pdev, drv_priv);

	ret = request_irq(pdev->irq, channel_irq_handler, IRQF_SHARED,
			DRIVER_NAME, &drv_priv->peer);
	if (ret) {
		pr_err("[GCH] request_irq failed. pdev->irq: %d\n", pdev->irq);
		goto iomap_release;
	}
	pr_info("[GCH] request_irq done");

	pr_info("[GCH] DYN_ALLOC_REQ_TEST: send signal to peer_id %d", 0);

	shrm_ipa = mmio_read_to_get_shrm();
	if (shrm_ipa <= 0) {
		pr_err("[GCH] %s failed to get shrm_ipa with %d", __func__, shrm_ipa);
	}

	ret = add_shrm_chunk(shrm_ipa);
	if (ret) { //for the test
		pr_err("[GCH] %s add_shrm_chunk() is failed with %d\n", __func__, ret);
	}

	/*
	pr_info("[GCH] TEST: send signal to destination peer_id %d", drv_priv->peer.id);
	send_signal(drv_priv->peer.id);
	*/

	/*
	if (drv_priv->role == CLIENT) {
		pr_info("[GCH] start schedule_work for send");
		schedule_work(&drv_priv->send);
	}
	*/

	pr_info("[GCH] %s done\n", __func__);

    return 0;

iomap_release:
	pci_iounmap(pdev, drv_priv->ioeventfd_addr);
pci_disable:
	pci_disable_device(pdev);
	return ret;
}

/*
 * About inter co-resident realm shared memory
 * 1. Setup the first shared memory before REALM_ACTIVATE state
 * 2. Runtime shared memory allocation request
 */

static void channel_remove(struct pci_dev *pdev)
{
	if (drv_priv) {
		kfree(drv_priv);
	}

	free_irq(pdev->irq, pdev);
	pci_iounmap(pdev, drv_priv->ioeventfd_addr);
	/* Free memory region */
	pci_release_region(pdev, 0);
	pci_disable_device(pdev);
}




MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunwook Eom <speed.eom@samsung.com>");
MODULE_VERSION("0.1");

module_init(channel_init);
module_exit(channel_exit);
