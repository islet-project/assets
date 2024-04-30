/* Sample Linux PCI device driver */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "host_channel"
#define PEER_LIST_MAX  128
#define MINOR_BASE 0
#define MINOR_NUM 1

#define HOST_CHANNEL_ID 0

static int dev_major_num;

struct peer {
    int id;      /* This is for identifying peers. It's NOT vmid */
    int eventfd;
};

/* This is a "private" data structure */
struct channel_priv {
	int id;
	int eventfd; 
	int cnt;
	struct peer peers[PEER_LIST_MAX];
};

// TODO: Need lock
static struct channel_priv drv_priv;

static struct cdev ch_cdev;
static struct class *ch_class;

static int search_peer_idx(int peer_id) {
    for (int i = 0; i < drv_priv.cnt; i++) {
        if (peer_id == drv_priv.peers[i].id) {
            return i;
        }
    }
    return -1;
}

static bool inline is_new_peer(int peer_id) {
	return search_peer_idx(peer_id) == -1 ? true : false;
}

static bool push_back(struct peer new_peer) {
    if (drv_priv.cnt >= PEER_LIST_MAX) {
        return false;
    }
	pr_info("[ID:%d] push_back peer.id %d, peer.eventfd %d",
            drv_priv.id, new_peer.id, new_peer.eventfd);
    drv_priv.peers[drv_priv.cnt++] = new_peer;
    return true;
}

static int channel_open(struct inode *inode, struct file *file)
{
	struct peer *peer;
	pr_info("CHANNEL: device opened\n");

	peer = kmalloc(sizeof(struct peer), GFP_KERNEL);
    if (!peer) {
		pr_err("CHANNEL: kmalloc failed");
        return -ENOMEM;
    }

    file->private_data = peer;

    return 0;
}

static int channel_release(struct inode * inode, struct file * file)
{
    pr_info("CHANNEL: channel_release start");
    if (file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

static ssize_t channel_write(struct file *file, const char __user *user_buffer, size_t size, loff_t * offset)
{
	int ret = 0;
    struct peer *peer = (struct peer*) file->private_data;
    ssize_t len = sizeof(struct peer) - *offset;

    if (len <= 0)
        return 0;

    /* read data from user buffer to my_data->buffer */
    ret = copy_from_user(peer + *offset, user_buffer, len);
    if (ret) {
		return -EFAULT;
	}

	pr_info("CHANNEL: channel_write done. peer's id: %d, eventfd: %d len: %d\n", peer->id, peer->eventfd, len);
	// TODO: add peer to the peer list
	if (drv_priv.id < 0 || drv_priv.eventfd < 0) {
		drv_priv.id = peer->id;
		drv_priv.eventfd = peer->eventfd;
	} else if (is_new_peer(peer->id)) {
		ret = push_back(*peer);
		if (ret) {
			pr_err("failed to push_back a new peer. id: %d, eventfd: %d", peer->id, peer->eventfd);
		}
	} else {
		pr_err("The peer already exists: id: %d, eventfd: %d", peer->id, peer->eventfd);
	}

    *offset += len;
    return len;
}

static const struct file_operations ch_fops = {
	.owner   = THIS_MODULE,
	.open	= channel_open,
	.write   = channel_write,
	.release = channel_release,
};

static int __init channel_init(void)
{
	int ret = 0;
	struct device *dev_struct;
	dev_t ch_dev;

	pr_info("CHANNEL: channel_init start \n");

	ret = alloc_chrdev_region(&ch_dev, MINOR_BASE, MINOR_NUM, DEVICE_NAME);
	if (ret) {
		pr_err("%s alloc_chrdev_region failed %d\n", __func__, ret);
		return ret;
	}

	cdev_init(&ch_cdev, &ch_fops);

	ret = cdev_add(&ch_cdev, ch_dev, MINOR_NUM);
	if (ret) {
		pr_err("%s cdev_add failed %d\n", __func__, ret);
		goto unreg_dev_num;
	}

	ch_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(ch_class)) {
		pr_err("%s class_create failed\n", __func__);
		goto unreg_cdev;
	}

	dev_struct = device_create(ch_class, NULL, ch_dev, NULL, DEVICE_NAME);
	if (IS_ERR(dev_struct)) {
		pr_err("%s device_create failed\n", __func__);
		goto unreg_class;
	}

	memset(&drv_priv, -1, sizeof(drv_priv));

	dev_major_num = MAJOR(ch_dev);
	pr_info("%s major:minor = %d:%d\n", __func__, dev_major_num, MINOR(ch_dev));

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
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunwook Eom <speed.eom@samsung.com>");
MODULE_VERSION("0.1");

module_init(channel_init);
module_exit(channel_exit);
