/* Sample Linux PCI device driver */

#include <linux/init.h>
#include <linux/module.h>
#include <string.h>

#define DRIVER_NAME "host_channel"
#define PEER_LIST_MAX  128
#define MINOR_BASE 0

#define HOST_CHANNEL_ID 0

static int dev_major_num;

struct peer {
    int id;      /* This is for identifying peers. It's NOT vmid */
    int eventfd;
}

/* This is a "private" data structure */
struct channel_priv {
	int id;
	int eventfd; 
	int cnt;
	struct peer peers[PEER_LIST_MAX];
};

// TODO: Need lock
static struct channel_priv drv_priv;

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

static int channel_write(struct file *file, const char __user *user_buffer,
                    size_t size, loff_t * offset)
{
	int ret = 0;
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
	if (drv_priv.id == -1 || drv_priv.eventfd == -1) {
		drv_priv.id = peer.id;
		drv_priv.eventfd = peer.eventfd;
	} else if (is_new_peer(peer.id)) {
		ret = push_back(peer);
		if (ret) {
			pr_err("failed to push_back a new peer. id: %d eventfd: %d", peer.id, peer.eventfd);
		}
	}

    *offset += len;
    return len;
}

static int __init channel_init(void)
{
	int ret = 0;

	/* Register device node ops. */
	ret = register_chrdev(0, DRIVER_NAME, &channel_ops);
	if (ret < 0) {
		pr_err("CHANNEL: register_chrdev failed %d\n", ret);
		return ret;
	}
	memset(&drv_priv, -1, sizeof(drv_priv));

	dev_major_num = ret;
	pr_info("CHANNEL: major device numer: %d\n", dev_major_num);

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
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunwook Eom <speed.eom@samsung.com>");
MODULE_VERSION("0.1");

module_init(channel_init);
module_exit(channel_exit);
