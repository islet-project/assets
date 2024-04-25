/* Sample Linux PCI device driver */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define DRIVER_NAME "channel"
#define VENDOR_ID 0x1af4
#define DEVICE_ID 0x10f0
#define PEER_LIST_MAX  128

#define MINOR_BASE 0

static int dev_major_num;

/* This sample driver supports device with VID = 0x010F, and PID = 0x0F0E*/
static struct pci_device_id channel_id_table[] = {
    { PCI_DEVICE(0x1af4, 0x10f0) },
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

static const struct file_operations channel_ops = {
	.owner   = THIS_MODULE,
	.open	= channel_open,
	.write   = channel_write,
	.release = channel_release,
};

static int channel_open(struct inode *inode, struct file *file)
{
	pr_info("CHANNEL: device opened\n");
	struct peer *peer = kmalloc(sizeof(struct peer), GFP_KERNEL);

    if (!peer) {
		pr_err("CHANNEL: kmalloc failed");
        return -ENOMEM;
    }

    file->private_data = peer;

    return 0;
}

static int channel_release(struct inode * inode, struct file * filp)
{
    pr_info("CHANNEL: channel_release start");
    if (file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

static int channel_write(struct file *file, const char __user *user_buffer,
                    size_t size, loff_t * offset)
{
    struct peer *peer= (struct peer*) file->private_data;
    ssize_t len = min(sizeof(struct peer) - *offset, size);

    if (len <= 0)
        return 0;

    /* read data from user buffer to my_data->buffer */
    if (copy_from_user(peer + *offset, user_buffer, len))
        return -EFAULT;

	pr_info("CHANNEL: channel_write done. peer: %d %d %d len: %d",
			peer->id, peer->sock_fd, peer->eventfd, len);
	// TODO: add peer to the peer list

    *offset += len;
    return len;
}

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
	struct peer_list peer_list;
};

static int __init channel_init(void)
{
	int ret = 0;

	/* Register device node ops. */
	ret = register_chrdev(0, DRIVER_NAME, &channel_ops);
	if (ret) {
		pr_err("CHANNEL: register_chrdev failed %d\n", ret);
		return ret;
	}

	dev_major_num = ret;
	pr_info("CHANNEL: major device numer: %d\n", dev_major_num);

	ret = pci_register_driver(&channel_driver);
	if (ret) {
		pr_err("CHANNEL: pci_register_driver failed %d\n", ret);
		goto err;
	}

	return 0;

err:
	unregister_chrdev(dev_major_num, DRIVER_NAME);
	return ret;
}

static void __exit channel_exit(void)
{
    int minor; 
    dev_t dev = MKDEV(dev_major_num, MINOR_BASE);

	unregister_chrdev(dev_major_num, DRIVER_NAME);
    pci_unregister_driver(&channel_driver);
}

void release_device(struct pci_dev *pdev)
{
    free_irq(pdev->irq, pdev);
    pci_disable_device(pdev);
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
    int ret;
    u16 vendor, device;
    unsigned long mmio_start,mmio_len;
    struct channel_priv *drv_priv;

    /* Let's read data from the PCI device configuration registers */
    pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
    pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

    pr_info("device vid: 0x%X pid: 0x%X\n", vendor, device);

    ret = pci_enable_device(pdev);
    if (ret) {
        return ret;
    }

    /* Allocate memory for the driver private data */
    drv_priv = kzalloc(sizeof(struct channel_priv), GFP_KERNEL);

    if (!drv_priv) {
        release_device(pdev);
        return -ENOMEM;
    }

    /* Set driver private data */
    /* Now we can access mapped "hwmem" from the any driver's function */
    pci_set_drvdata(pdev, drv_priv);

	ret = request_irq(pdev->irq, channel_irq_handler, IRQF_SHARED,
			DRIVER_NAME, &drv_priv->peer_list);
	if (ret) {
		pr_err("CHANNEL: request_irq failed. pdev->irq: %d\n", pdev->irq);
	}

    return 0;
}

static void channel_remove(struct pci_dev *pdev)
{
    struct channel_priv *drv_priv = pci_get_drvdata(pdev);

    if (drv_priv) {
        kfree(drv_priv);
    }

    release_device(pdev);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunwook Eom <speed.eom@samsung.com>");
MODULE_VERSION("0.1");

module_init(channel_init);
module_exit(channel_exit);
