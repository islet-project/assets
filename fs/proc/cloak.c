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
#include <linux/completion.h>
#include <asm/uaccess.h>

#define DEVICE_NAME       "cloak_host"       /* Name of device in /proc/devices */

static int cloak_host_device_major;              /* Major number assigned to our device driver */
static struct class *cloak_host_cls;

struct cloak_completion {
	struct completion comp;
	unsigned long arg;
};
struct cloak_completion comp_cloak_gw_recv;  // completion object to wait and wake-up for virtio requests. gw receiving...
struct cloak_completion comp_cloak_tx_gw_send;  // gw sending..
struct cloak_completion comp_cloak_rx_gw_send;

unsigned long cloak_host_call_response = -1;

#define CLOAK_MSG_TYPE_VSOCK_TX (8)
#define CLOAK_MSG_TYPE_VSOCK_RX (9)
#define CLOAK_MSG_TYPE_VSOCK_RX_RESP (19)

#define RECORD_CLOAK_MSG_TYPE (9999)
#define RECEIVE_MSG_IN_KERNEL (19998)
#define SEND_MSG_IN_KERNEL (19999)

unsigned long wait_gw_recv(void) {
	struct cloak_completion *comp = &comp_cloak_gw_recv;

	//pr_info("[Cloak] RECEIVE_MSG_IN_KERNEL. wait a request..\n");
	wait_for_completion(&comp->comp);
	//pr_info("[Cloak] received a request %d\n", comp->arg);
	return comp->arg;
}

void complete_gw_recv(unsigned long arg) {
	struct cloak_completion *comp = &comp_cloak_gw_recv;
	comp->arg = arg;
	complete(&comp->comp);
}

void wait_gw_send(bool is_tx) {
	struct cloak_completion *comp = NULL;
	if (is_tx)
		comp = &comp_cloak_tx_gw_send;
	else
		comp = &comp_cloak_rx_gw_send;

	//pr_info("[Cloak] wait a request.. gw send\n");
	wait_for_completion(&comp->comp);
	//pr_info("[Cloak] wait done %d\n", comp->arg);
}

void complete_gw_send(unsigned long arg, bool is_tx) {
	struct cloak_completion *comp = NULL;
	if (is_tx)
		comp = &comp_cloak_tx_gw_send;
	else
		comp = &comp_cloak_rx_gw_send;

	comp->arg = arg;
	complete(&comp->comp);
}

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
	switch (cmd) {
		case RECORD_CLOAK_MSG_TYPE:
			cloak_host_call_response = arg;
			break;
		case RECEIVE_MSG_IN_KERNEL: {
			unsigned long res = wait_gw_recv();
			int type = (int)res;
			int ret = 0;

			ret = copy_to_user((int *)arg, (int *)&type, sizeof(type));
			if (ret != 0) {
				pr_info("[Cloak] ioctl: copy_to_user error: %d\n", ret);
			}
			break;
		}
		case SEND_MSG_IN_KERNEL: {
			if (arg == CLOAK_MSG_TYPE_VSOCK_TX)
				complete_gw_send(CLOAK_MSG_TYPE_VSOCK_TX, true);
			else if (arg == CLOAK_MSG_TYPE_VSOCK_RX)
				complete_gw_send(CLOAK_MSG_TYPE_VSOCK_RX, false);
			break;
		}
		default:
			break;
	}
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

	memset(&comp_cloak_gw_recv, 0, sizeof(comp_cloak_gw_recv));
	memset(&comp_cloak_tx_gw_send, 0, sizeof(comp_cloak_tx_gw_send));
	memset(&comp_cloak_rx_gw_send, 0, sizeof(comp_cloak_rx_gw_send));

	init_completion(&comp_cloak_gw_recv.comp);
	init_completion(&comp_cloak_tx_gw_send.comp);
	init_completion(&comp_cloak_rx_gw_send.comp);
	pr_info("Device created on /dev/%s\n", DEVICE_NAME);
	return 0;
}

fs_initcall(cloak_host_init);
