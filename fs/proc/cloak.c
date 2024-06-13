#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <asm/uaccess.h>

#define DEVICE_NAME       "cloak_host"       /* Name of device in /proc/devices */

static int cloak_host_device_major;              /* Major number assigned to our device driver */
static struct class *cloak_host_cls;

unsigned long cloak_host_call_response = -1;

static int cloak_host_open(struct inode *i, struct file *f)
{
	return 0;
}

static int cloak_host_release(struct inode *i, struct file *f)
{
	return 0;
}
static long cloak_host_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	//pr_info("cloak_host_ioctl: %d-%d\n", cmd, arg);
	cloak_host_call_response = arg;
	return 0;
}

static struct file_operations cloak_host_fops = {
	.open = cloak_host_open,
	.release = cloak_host_release,
	.unlocked_ioctl = cloak_host_ioctl,
};

static int __init cloak_host_init(void)
{
	cloak_host_device_major = register_chrdev(0, DEVICE_NAME, &cloak_host_fops);
	if (cloak_host_device_major < 0) {
		pr_info("register_chrdev failed with %d\n", cloak_host_device_major);
		return cloak_host_device_major;
	}
	pr_info("Chardev registered with major %d\n", cloak_host_device_major);

	cloak_host_cls = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(cloak_host_cls, NULL, MKDEV(cloak_host_device_major, 0), NULL, DEVICE_NAME);

	pr_info("Device created on /dev/%s\n", DEVICE_NAME);
	return 0;
}

fs_initcall(cloak_host_init);
