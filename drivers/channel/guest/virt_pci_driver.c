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
#include "channel.h"
#include "shrm.h"
#include "virt_pci_driver.h"

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

struct peer {
    int id;      /* This is for identifying peers. It's NOT vmid */
    int eventfd;
};

/* This is a "private" data structure */
/* You can store there any data that should be passed between driver's functions */
struct channel_priv {
	int vmid;
	ROLE role;
	uint32_t* ioeventfd_addr;
	struct peer peer;
	struct work_struct send, receive;
	struct work_struct rt_sender, rt_receiver; // for the dyn_shrm_remove_test
	//struct work_struct setup_rw_rings, setup_ro_rings;
	struct work_struct setup_rw_rings;
	u64* mapped_bar_addr;
	u64 *rw_shrm_va_start;
	u64 *ro_shrm_va_start;
	struct shrm_list* rw_shrms;
	struct list_head ro_shrms;
	struct rings_to_send* rts;
	struct rings_to_receive* rtr;
};



static u64 get_shrm_ipa_start(SHRM_TYPE shrm_type) {
	return (shrm_type == SHRM_RW) ? SHRM_RW_IPA_REGION_START : SHRM_RO_IPA_REGION_START;
}

u64* get_shrm_va(SHRM_TYPE shrm_type, u64 ipa) {
	u64 shrm_va = (shrm_type == SHRM_RW) ? (u64)drv_priv->rw_shrm_va_start : (u64)drv_priv->ro_shrm_va_start ;

	if (!shrm_va) {
		pr_err("%s shrm_va shouldn't be non-zero. shrm_type %d. ipa 0x%llx",
				__func__, shrm_type, ipa);
		return NULL;
	}

	shrm_va += ipa % SHRM_IPA_RANGE_SIZE;
	return (u64*)shrm_va;
}

void send_signal(int peer_id) {
	pr_info("[GCH] write %d to ioeventfd_addr 0x%x", peer_id, (uint32_t)drv_priv->ioeventfd_addr);
	iowrite32(peer_id, drv_priv->ioeventfd_addr);
}

void notify_peer(void) {
	send_signal(drv_priv->peer.id);
}

static void get_cur_vmid(void) {
	int vmid;

	if (drv_priv->vmid) {
		pr_info("[GCH] %s vmid is already set %d",__func__, drv_priv->vmid);
		return;
	}
	
	vmid = readl(drv_priv->mapped_bar_addr + BAR_MMIO_CURRENT_VMID);

	if (vmid == INVALID_PEER_ID || vmid == SHRM_ALLOCATOR) {
		pr_info("[GCH] The vmid is not valid %d", vmid);
	} else {
		pr_info("[GCH] get vmid %d", vmid);
		drv_priv->vmid = vmid;
		drv_priv->role = (vmid == CLIENT) ? CLIENT : SERVER;
		pr_info("[GCH] my role is %d", drv_priv->role);
	}
}

static int set_peer_id(void) {
	int peer_id;

	if (drv_priv->peer.id) {
		pr_info("[GCH] %s peer id is already set %d",__func__, drv_priv->peer.id);
		return peer_id;
	}
	
	peer_id = readl(drv_priv->mapped_bar_addr + BAR_MMIO_PEER_VMID);

	if (peer_id == INVALID_PEER_ID || peer_id == SHRM_ALLOCATOR) {
		pr_info("[GCH] peer_id is not valid %d", peer_id);
	} else {
		pr_info("[GCH] get peer_id %d", peer_id);
		drv_priv->peer.id = peer_id;
	}

	return peer_id;
}

static u64 mmio_read(u64 offset) {
	u64 ret;

	switch(offset) {
		case BAR_MMIO_CURRENT_VMID:
		case BAR_MMIO_PEER_VMID:
		case BAR_MMIO_SHM_RW_IPA_BASE:
		case BAR_MMIO_SHM_RO_IPA_BASE:
			ret = readl(drv_priv->mapped_bar_addr + offset);
			return ret;
		default:
			pr_err("%s wrong mmio offset 0x%llx", __func__, offset);
			return 0;
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

s64 mmio_read_to_get_shrm(SHRM_TYPE shrm_type) {
	u64 mmio_type = (shrm_type == SHRM_RW) ? BAR_MMIO_SHM_RW_IPA_BASE : BAR_MMIO_SHM_RO_IPA_BASE;

	return mmio_read(mmio_type);
}

int mmio_write_to_remove_shrm(u64 ipa) {
	return mmio_write(BAR_MMIO_SHM_RW_IPA_BASE, ipa);
}

int mmio_write_to_unmap_shrm(u64 ipa) {
	return mmio_write(BAR_MMIO_UNMAP_SHRM_IPA, ipa);
}

int mmio_write_to_get_ro_shrm(u32 shrm_id) {
	return mmio_write(BAR_MMIO_SHM_RO_IPA_BASE, shrm_id);
}

static u64 get_reserved_rw_shrm_ipa(void) {
	u64 mmio_ret, reserved_shrm_ipa = 0, shrm_id;

	mmio_ret = mmio_read_to_get_shrm(SHRM_RW);
	if (!mmio_ret) {
		pr_err("[GCH] %s failed to get shrm_ipa with %d", __func__, mmio_ret);
		return 0;
	}

	reserved_shrm_ipa = mmio_ret & ~SHRM_ID_MASK;
	shrm_id = mmio_ret & SHRM_ID_MASK;

	// reserved_shrm_ipa should be the first requested shrm chunk
	if (reserved_shrm_ipa != RESERVED_SHRM_RW_IPA_REGION_START) {
		pr_err("[GCH] %s: invalid reserved_shrm_ipa. %llx != %llx",
				__func__, reserved_shrm_ipa, RESERVED_SHRM_RW_IPA_REGION_START);
		return 0;
	}
	return reserved_shrm_ipa;
}

static void drv_setup_rw_rings(struct work_struct *work) {
	int ret = 0;
	u64 shrm_rw_ipa_range_start, reserved_shrm_ipa;

	shrm_rw_ipa_range_start = get_shrm_ipa_start(SHRM_RW);
	if (!shrm_rw_ipa_range_start) {
		pr_err("%s: get_shrm_ipa_start() failed", __func__);
		return;
	}

	drv_priv->rw_shrm_va_start = memremap(shrm_rw_ipa_range_start, SHRM_IPA_RANGE_SIZE, MEMREMAP_WB);
	if (!drv_priv->rw_shrm_va_start) {
		pr_err("%s memremap for 0x%llx failed \n", __func__, shrm_rw_ipa_range_start);
		return;
	}

	drv_priv->rts = kzalloc(sizeof(*drv_priv->rts), GFP_KERNEL);
	if (!drv_priv->rts) {
		pr_err("[GCH] failed to kzalloc for drv_priv->rts");
		return;
	}

	drv_priv->rtr = kzalloc(sizeof(*drv_priv->rtr), GFP_KERNEL);
	if (!drv_priv->rtr) {
		pr_err("[GCH] failed to kzalloc for drv_priv->rts");
		return;
	}

	reserved_shrm_ipa = get_reserved_rw_shrm_ipa();
	if (!reserved_shrm_ipa) {
		pr_err("%s: reserved_shrm_ipa() failed", __func__);
		return;
	}

	ret = init_rw_rings(drv_priv->rts, drv_priv->rtr, reserved_shrm_ipa);
	if (ret) {
		pr_err("%s: init_rw_rings failed. %d", __func__, ret);
		return;
	}

	drv_priv->rw_shrms = init_shrm_list(RESERVED_SHRM_RW_IPA_REGION_END, SHRM_IPA_RANGE_SIZE);
	if (!drv_priv->rw_shrms) {
		pr_err("[GCH] %s: init_shrm_list() failed", __func__);
		return;
	}

	do {
		ret = req_shrm_chunk(drv_priv->rts, drv_priv->rw_shrms);
	} while(ret == -EAGAIN);
}

//TODO: implement it!!
static u64 get_reserved_ro_shrm_ipa(void) {
	u64 first_peer_shrm_id = set_peer_id(), reserved_shrm_ro_ipa = 0;

	reserved_shrm_ro_ipa = req_ro_shrm_ipa(first_peer_shrm_id);
	if (!reserved_shrm_ro_ipa)
		pr_err("%s: req_ro_shrm_ipa() failed with first_peer_shrm_id %d", __func__, first_peer_shrm_id);

	return reserved_shrm_ro_ipa;
}

static int drv_setup_ro_rings(void) {
	u64 reserved_shrm_ro_ipa;
	u64 shrm_ro_ipa_region_start = get_shrm_ipa_start(SHRM_RO);
	int ret;

	pr_info("[GCH] %s start. And my role: %d", __func__, drv_priv->role);
	if (drv_priv->role != SERVER) {
		pr_err("[GCH] My role is not SERVER but %d", drv_priv->role);
		return -1;
	}
	copy_from_shrm(NULL, NULL); // for the build test

	INIT_LIST_HEAD(&drv_priv->ro_shrms);

	drv_priv->ro_shrm_va_start = memremap(shrm_ro_ipa_region_start, SHRM_IPA_RANGE_SIZE, MEMREMAP_WB);
	if (!drv_priv->ro_shrm_va_start) {
		pr_err("%s memremap for 0x%llx failed \n", __func__, shrm_ro_ipa_region_start);
		return -2;
	}
	pr_info("[GCH] %s memremap result: 0x%llx for [0x%llx:0x%llx)", __func__,
			drv_priv->ro_shrm_va_start, shrm_ro_ipa_region_start, shrm_ro_ipa_region_start + SHRM_IPA_RANGE_SIZE);

	reserved_shrm_ro_ipa = get_reserved_ro_shrm_ipa();
	if (!reserved_shrm_ro_ipa)
		return -3;

	//pr_info("[GCH] %s call set_memory_shared with shrm_ipa 0x%llx va: %p", __func__, shrm_ipa, drv_priv->ro_shrm_va_start);
	//set_memory_shared(shrm_ipa, 1); // don't need it. cos the mapping is already done using mmap write trap

	ret = init_ro_rings(drv_priv->rts, drv_priv->rtr, reserved_shrm_ro_ipa);
	if (ret) {
		pr_err("%s: init_rw_rings failed. %d", __func__, ret);
		return -4;
	}

	pr_info("%s done", __func__);
	return 0;
}

static void ch_send(struct work_struct *work) {
	u64 msg = 0xBEEF;
	u64 *rw_shrm_va = get_shrm_va(SHRM_RW, test_shrm_offset);
	//u64 *rw_shrm_va = drv_priv->rw_shrm_va_start;

	pr_info("%s: front: %d, rear: %d",
			__func__, drv_priv->rts->avail->front, drv_priv->rts->avail->rear);
	set_peer_id();

	pr_err("[GCH] %s start. And my role: %d", __func__, drv_priv->role);
	if (drv_priv->role != CLIENT) {
		pr_err("[GCH] My role is not CLIENT but %d", drv_priv->role);
		return;
	}

	if (!rw_shrm_va) {
		pr_err("[GCH] %s rw_shrm_va shouldn't be NULL", __func__);
		return;
	}

	pr_info("[GCH] %s drv_priv->rw_shrm_va_start 0x%llx, rw_shrm_va: 0x%llx",
			__func__, drv_priv->rw_shrm_va_start, rw_shrm_va);

	write_packet(drv_priv->rts, drv_priv->rw_shrms, &msg, sizeof(u64));
	pr_info("%s: front: %d, rear: %d",
			__func__, drv_priv->rts->avail->front, drv_priv->rts->avail->rear);

	pr_info("[GCH] %s done", __func__);
}

static void ch_receive(struct work_struct *work) {
	int ret;

	pr_info("%s start", __func__);

	if (!drv_priv->ro_shrm_va_start) {
		ret = drv_setup_ro_rings();
		if (ret) {
			pr_err("%s: drv_setup_ro_rings failed %d", __func__, ret);
			return;
		}
	}

	ret = read_packet(drv_priv->rtr, &drv_priv->ro_shrms);
	if (ret) {
		pr_err("%s: read_packet failed %d", __func__, ret);
		return;
	}
	pr_info("%s done", __func__);
}


static void dyn_rt_sender(struct work_struct *work) {
	u64 rw_shrm_ipa = get_shrm_ipa_start(SHRM_RW) + test_shrm_offset;

	pr_info("%s rw_shrm_ipa: 0x%llx", __func__, rw_shrm_ipa);

	remove_shrm_chunk(drv_priv->rw_shrms, rw_shrm_ipa);
	pr_info("%s done", __func__);
}

static void dyn_rt_receiver(struct work_struct *work) {
	u64 ro_shrm_ipa = get_shrm_ipa_start(SHRM_RO) + test_shrm_offset;

	pr_info("%s mmio_write_to_unmap_shrm start", __func__);
	mmio_write_to_unmap_shrm(ro_shrm_ipa);
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
	INIT_WORK(&drv_priv->setup_rw_rings, drv_setup_rw_rings);
	//INIT_WORK(&drv_priv->setup_ro_rings, drv_setup_ro_rings);

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

	schedule_work(&drv_priv->setup_rw_rings);

	/*
	 * schedule_work(setup_rw_rings);
	 * schedule_work(setup_ro_rings); // if my role is Client, it starts after getting noti by peer
	 * schedule_work(setup_ro_rings); // if my role is Server, start it now 
	 */


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
