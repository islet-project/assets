/* Sample Linux PCI device driver */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define DRIVER_NAME "channel"
#define VENDOR_ID 0x1af4
#define DEVICE_ID 0x10f0
#define PEER_LIST_MAX  128

#define MINOR_BASE 0


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
    int sock_fd; /* connected unix sock */
    int eventfd;
}

struct peer_list {
	int cnt;
	struct peer peers[PEER_LIST_MAX];
}

/* This is a "private" data structure */
/* You can store there any data that should be passed between driver's functions */
struct channel_priv {
	uint32_t ioeventfd_addr;
	uint32_t ioeventfd_size;
	void *ioeventfd_iomap_addr;
	struct peer_list peer_list;
};

static int __init channel_init(void)
{
	int ret = 0;

	pr_info("[CH] %s start\n", __func__);
	ret = pci_register_driver(&channel_driver);
	if (ret) {
		pr_err("[CH] pci_register_driver failed %d\n", ret);
	}
	return ret;
}

static void __exit channel_exit(void)
{
	pr_info("[CH] %s start\n", __func__);
    pci_unregister_driver(&channel_driver);
}

/* 
 * There are two differnt logic based by running layer (Host or Realm)
 * Host: Send dyn memory or Retrieve it from realm
 * Realm: Get dyn memory or Receive I/O ring request 
 */
static irqreturn_t channel_irq_handler(int irq, struct peer_list *peer_list)
{
   printk("Handle IRQ #%d\n", irq);
   return IRQ_HANDLED;
}

/* This function is called by the kernel */
static int channel_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int bar = 0, ret;
    u16 vendor, device;
    uint64_t dev_ioeventfd_addr, dev_ioeventfd_size;
    struct channel_priv *drv_priv;

    pr_info("[CH] %s start\n", __func__);

    /* Let's read data from the PCI device configuration registers */
    pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
    pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

    pr_info("[CH] device vid: 0x%X pid: 0x%X\n", vendor, device);

    ret = pci_enable_device(pdev);
    if (ret) {
		pr_err("[CH] pci_enable_device failed %d\n", ret);
        return ret;
    }

    /* Allocate memory for the driver private data */
    drv_priv = kzalloc(sizeof(struct channel_priv), GFP_KERNEL);
    if (!drv_priv) {
		pr_err("[CH] failed to kzalloc for drv_priv\n");
		goto pci_disable;
    }

	/* Request memory region for the BAR */
    ret = pci_request_region(pdev, bar, DRIVER_NAME);
    if (ret) {
		pr_err("[CH] pci_request_region failed %d\n", ret);
		goto pci_disable;
    }

	dev_ioeventfd_addr = pci_resource_start(pdev, bar);
	dev_ioeventfd_size = pci_resource_len(pdev, bar);
	drv_priv.ioeventfd_addr = pci_iomap(pdev, bar, 0);

	pr_info("[CH] ioeventfd addr 0x%x, size 0x%x, iomap_addr 0x%lx",
			dev_ioeventfd_addr, dev_ioeventfd_size, (uint64_t)drv_priv.ioeventfd_addr);
	if (!drv_priv.ioeventfd_iomap_addr) {
		pr_err("[CH] pci_iomap failed for ioeventfd_iomap_addr\n");
		goto pci_release;
	}

    /* Set driver private data */
    /* Now we can access mapped "hwmem" from the any driver's function */
    pci_set_drvdata(pdev, drv_priv);

	ret = request_irq(pdev->irq, channel_irq_handler, IRQF_SHARED,
			DRIVER_NAME, &drv_priv->peer_list);
	if (ret) {
		pr_err("CHANNEL: request_irq failed. pdev->irq: %d\n", pdev->irq);
		goto iomap_release;
	}

	pr_info("[CH] %s done\n", __func__);

    return 0;

iomap_release:
	pci_iounmap(pdev, drv_priv.ioeventfd_addr);
pci_release:
	pci_release_region(pdev, bar);
pci_disable:
	pci_disable_device(pdev);
	return -EBUSY;
}

static void channel_remove(struct pci_dev *pdev)
{
	struct channel_priv *drv_priv = pci_get_drvdata(pdev);

	if (drv_priv) {
		kfree(drv_priv);
	}

	free_irq(pdev->irq, pdev);
	pci_iounmap(pdev, drv_priv.ioeventfd_addr);
	/* Free memory region */
	pci_release_region(pdev, bar);
	pci_disable_device(pdev);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunwook Eom <speed.eom@samsung.com>");
MODULE_VERSION("0.1");

module_init(channel_init);
module_exit(channel_exit);
