/* Sample Linux PCI device driver */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/rsi_cmds.h>
#include <linux/workqueue.h>
#include <linux/io.h>

#define DRIVER_NAME "guest_channel"
#define VENDOR_ID 0x1af4
#define DEVICE_ID 0x1110 // temporarily uses ivshmem's device id
#define MINOR_BASE 0

#define IOEVENTFD_BASE_ADDR 0x7fffff00
#define IOEVENTFD_BASE_SIZE 0x100

#define SHM_RW_BASE_IPA 0xC0000000
#define SHM_RO_BASE_IPA 0xD0000000

#define BAR_MMIO_OFFSET_PEER_VMID 0
#define BAR_MMIO_OFFSET_SHM_RO_IPA_BASE 8

#define INVALID_PEER_ID -1

struct channel_priv *drv_priv;
void set_memory_shared(phys_addr_t start, int numpages);

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
	struct peer peer;
	ROLE role;
	struct work_struct send;
	struct work_struct receive;
	u64* shm_base_va;
	u64* shm_ro_base_va;
	u64* mapped_bar_addr;
};

static void set_peer_id(void) {
	int peer_id;

	if (drv_priv->peer.id)
		return;
	
	peer_id = readl(drv_priv->mapped_bar_addr + BAR_MMIO_OFFSET_PEER_VMID);

	if (peer_id == INVALID_PEER_ID || peer_id == SHM_ALLOCATOR) {
		pr_info("[GCH] peer_id is not valid %d", peer_id);
	} else {
		pr_info("[GCH] get peer_id %d", peer_id);
		drv_priv->peer.id = peer_id;
		drv_priv->role = (peer_id == CLIENT) ? SERVER: CLIENT;
		pr_info("[GCH] my role is %d", drv_priv->role);
	}
}

static void send_signal(int peer_id, uint32_t* ioeventfd_addr) {
	pr_info("[GCH] write %d to ioeventfd_addr 0x%x", peer_id, (uint32_t)ioeventfd_addr);
	iowrite32(peer_id, ioeventfd_addr);
}

static void ch_send(struct work_struct *work) {
	char msg[8] = "BEEF";

	set_peer_id();

	pr_err("[GCH] %s start. And my role: %d", __func__, drv_priv->role);
	if (drv_priv->role != CLIENT) {
		pr_err("[GCH] My role is not CLIENT but %d", drv_priv->role);
		return;
	}

	if (!drv_priv->shm_base_va) {
		pr_info("[GCH] %s call memremap() with SHM_RW_BASE_IPA %llx", __func__, SHM_RW_BASE_IPA);

		drv_priv->shm_base_va = memremap(SHM_RW_BASE_IPA, 0x1000, MEMREMAP_WB);
		if (!drv_priv->shm_base_va) {
			pr_err("%s memremap for 0x%llx failed \n", __func__, SHM_RW_BASE_IPA);
			return;
		}
		pr_info("[GCH] %s memremap() done. shm_base_va: 0x%llx", __func__, drv_priv->shm_base_va);

		pr_info("[GCH] %s call set_memory_shared with SHM_RW_BASE_IPA 0x%llx shm_base_va: 0x%llx",
				__func__, SHM_RW_BASE_IPA, drv_priv->shm_base_va);
		set_memory_shared(SHM_RW_BASE_IPA, 1);
	}

	strncpy((char*)drv_priv->shm_base_va, msg, sizeof(msg));
	pr_err("[GCH] %s strncpy msg to drv_priv->shm_base_va: %s\n", __func__, drv_priv->shm_base_va);
	send_signal(drv_priv->peer.id, drv_priv->ioeventfd_addr);
}

static void ch_receive(struct work_struct *work) {
	u64 shm_ro_base_ipa = 0;
	char msg[8];

	set_peer_id();

	pr_err("[GCH] %s start. And my role: %d", __func__, drv_priv->role);
	if (drv_priv->role != SERVER) {
		pr_err("[GCH] My role is not SERVER but %d", drv_priv->role);
		return;
	}

	if (!drv_priv->shm_ro_base_va) {
		shm_ro_base_ipa = readl(drv_priv->mapped_bar_addr + BAR_MMIO_OFFSET_SHM_RO_IPA_BASE);
		if (!shm_ro_base_ipa) {
			pr_err("[GCH] %s failed to get shm_ro_base_ipa", __func__);
			return;
		}

		pr_info("[GCH] %s call memremap() with shm_ro_base_ipa %llx", __func__, shm_ro_base_ipa);
		drv_priv->shm_ro_base_va = memremap(shm_ro_base_ipa, 0x1000, MEMREMAP_WB);
		if (!drv_priv->shm_ro_base_va) {
			pr_err("%s memremap for 0x%llx failed \n", __func__, shm_ro_base_ipa);
			return;
		}
		pr_info("[GCH] %s memremap() done. shm_ro_base_va: 0x%llx", __func__, drv_priv->shm_ro_base_va);

		//pr_info("[GCH] %s call set_memory_shared with shm_ro_base_ipa 0x%llx va: %p",
		//		__func__, shm_ro_base_ipa, drv_priv->shm_ro_base_va);
		//set_memory_shared(shm_ro_base_ipa, 1);
	}

	if (drv_priv->shm_ro_base_va)  {
		pr_err("[GCH] let's read the msg one by one:");
		for (int i; i < 4; i++) {
			char* ptr = (char*)drv_priv->shm_ro_base_va;
			pr_err("shm_ro_base_va[%d]: %c\n", i, ptr[i]);
		}

		pr_err("[GCH] strncpy start from shm_ro_base_va: %llx to msg\n", drv_priv->shm_ro_base_va);
		strncpy(msg, (const char *)drv_priv->shm_ro_base_va, sizeof(msg));
		pr_err("[GCH] %s msg read result: %s\n", __func__, msg);

		pr_err("[GCH] let's read the msg one by one:");
		for (int i; i < 4; i++) {
			char* ptr = (char*)drv_priv->shm_ro_base_va;
			pr_err("shm_ro_base_va[%d]: %c\n", i, ptr[i]);
		}
	} else {
		pr_err("[GCH] drv_priv->shm_ro_base_va is zero.\n");
	}
}

static int __init channel_init(void)
{
	int ret = 0;

	pr_info("[GCH] %s start\n", __func__);

	ret = pci_register_driver(&channel_driver);
	if (ret) {
		pr_err("[GCH] pci_register_driver failed %d\n", ret);
	}

	return ret;
}

static void __exit channel_exit(void)
{
	pr_info("[GCH] %s start\n", __func__);
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
		schedule_work(&drv_priv->receive);
	}

	return (cnt++) ? IRQ_NONE : IRQ_HANDLED;
	//return IRQ_HANDLED;
}

/* This function is called by the kernel */
static int channel_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int bar = 0, ret;
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
		return -1;
	}

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

	/*
	pr_info("[GCH] TEST: send signal to peer_id %d", 0);
	send_signal(0, drv_priv->ioeventfd_addr);

	pr_info("[GCH] TEST: send signal to destination peer_id %d", drv_priv->peer.id);
	send_signal(drv_priv->peer.id, drv_priv->ioeventfd_addr);
	*/

	if (drv_priv->role == CLIENT) {
		pr_info("[GCH] start schedule_work for send");
		schedule_work(&drv_priv->send);
	}

	pr_info("[GCH] %s done\n", __func__);

    return 0;

iomap_release:
	pci_iounmap(pdev, drv_priv->ioeventfd_addr);
pci_disable:
	pci_disable_device(pdev);
	return -EBUSY;
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
