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

#define DEVICE_NAME "host_channel"
#define MINOR_BASE 0
#define MINOR_NUM 1

#define WRITE_DATA_SIZE (sizeof(int)*2) // should be matched with Eventfd Allocator Server

static int dev_major_num;

/* This is a "private" data structure */
struct channel_priv {
	bool is_active; // TODO: do we need this ?
};

static struct channel_priv drv_priv;
static struct cdev ch_cdev;
static struct class *ch_class;

/*
 * Mark the drv_priv as inactive and schedule it for removal
 * assumes drv_priv.lock is held
 */
static void ch_deactivate(struct channel_priv *priv_ptr)
{
	BUG_ON(!priv_ptr->is_active);
	priv_ptr->is_active = false;
	// TODO: schedule_work(&ch_shutdown);
	// need to add eventfd_ctx_put(irqfd->eventfd); to ch_shutdown
}

static const struct file_operations ch_fops = {
	.owner   = THIS_MODULE,
	.open	= channel_open,
	.release = channel_release,
};

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
	drv_priv.is_active = true;

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
