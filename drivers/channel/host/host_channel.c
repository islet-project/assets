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
#define PEER_LIST_MAX  128
#define MINOR_BASE 0
#define MINOR_NUM 1

#define HOST_CHANNEL_ID 0
#define WRITE_DATA_SIZE (sizeof(int)*2) // should be matched with Eventfd Allocator Server

static int dev_major_num;

struct peer {
    int id;      /* This is for identifying peers. It's NOT vmid */
	int eventfd;
	struct eventfd_ctx *efd_ctx;
};

/* This is a "private" data structure */
struct channel_priv {
	spinlock_t lock;
	int id;
	int cnt; // peer count
	bool is_active;
	struct eventfd_ctx *efd_ctx;
	wait_queue_entry_t wait;
	poll_table pt;
	struct work_struct inject;
	struct peer peers[PEER_LIST_MAX];
};

static struct channel_priv drv_priv;
static struct cdev ch_cdev;
static struct class *ch_class;

// NOTE: drv_priv.lock must be held before calling this function
static int search_peer_idx(struct channel_priv* priv_ptr, int peer_id) {
    if (priv_ptr != &drv_priv) {
        return -1;
    }

    for (int i = 0; i < drv_priv.cnt; i++) {
        if (peer_id == drv_priv.peers[i].id) {
            return i;
        }
    }
    return -1;
}

// NOTE: drv_priv.lock must be held before calling this function
static bool inline is_new_peer(struct channel_priv* priv_ptr, int peer_id) {
	return search_peer_idx(priv_ptr, peer_id) == -1 ? true : false;
}

// NOTE: drv_priv.lock must be held before calling this function
static int push_back(struct channel_priv* priv_ptr, struct peer new_peer) {
    if (priv_ptr != &drv_priv || priv_ptr->cnt >= PEER_LIST_MAX) {
		pr_info("[CH] priv_ptr 0x%llx &drv_priv 0x%llx", (u64)priv_ptr, (u64)&drv_priv);
		pr_info("[CH] drv_priv.cnt %d", drv_priv.cnt);
        return -1;
    }
	pr_info("[CH] push_back peer.id %d, peer.eventfd %d peer.efd_ctx 0x%p",
			new_peer.id, new_peer.eventfd, new_peer.efd_ctx);
    priv_ptr->peers[drv_priv.cnt++] = new_peer;
    return 0;
}

static void ch_ptable_queue_proc(struct file *file, wait_queue_head_t *wqh, poll_table *pt)
{
	pr_info("[CH] %s add waitqueue entry to our eventfd->wqh", __func__);
	add_wait_queue_priority(wqh, &drv_priv.wait);
}

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


static void ch_inject(struct work_struct *work)
{
	struct peer peer;
	pr_info("[CH] %s start, drv_priv.cnt %d", __func__, drv_priv.cnt);
	if (!drv_priv.cnt) {
		return;
	}

	for (int i = 0; i < drv_priv.cnt; i++) {
		struct peer peer = drv_priv.peers[i];
		pr_info("[CH] Trigger eventfd_signal to peer's id %d, eventfd %d eventfd_ctx 0x%p",
				peer.id, peer.eventfd, peer.efd_ctx);
		eventfd_signal(peer.efd_ctx, 1);
	}

	pr_info("[CH] %s done", __func__);
}

/*
 * Called with wqh->lock held and interrupts disabled
 */
static int ch_wakeup(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
{
	struct channel_priv *priv_ptr = container_of(wait, struct channel_priv, wait);
	__poll_t flags = key_to_poll(key);
	int ret = 0;

	pr_info("[CH] %s start", __func__);
    if (priv_ptr != &drv_priv) {
		pr_err("[CH] %s Invalid priv_ptr\n", __func__);
        return -1;
    }

	if (flags & EPOLLIN) {
		u64 cnt;
		eventfd_ctx_do_read(priv_ptr->efd_ctx, &cnt);
		pr_info("[CH] %s EPOLLIN: eventfd cnt %d\n", __func__, cnt);
		//schedule_work(&check_mem_request()); TODO: need to implement check_mem_request() 

		schedule_work(&drv_priv.inject);
		ret = 1;
	}

	if (flags & EPOLLHUP) {
		/* The eventfd is closing, deactivate it */
		unsigned long flags;

		pr_info("[CH] %s eventfd is closed\n", __func__);

		spin_lock_irqsave(&priv_ptr->lock, flags);
		if (priv_ptr->is_active)
			ch_deactivate(priv_ptr);
		spin_unlock_irqrestore(&priv_ptr->lock, flags);
	}

	return ret;
}

static int channel_open(struct inode *inode, struct file *file)
{
	struct peer *peer;
	pr_info("[CH] device opened\n");

	peer = kmalloc(sizeof(struct peer), GFP_KERNEL);
    if (!peer) {
		pr_err("[CH] kmalloc failed");
        return -ENOMEM;
    }

    file->private_data = peer;

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

static ssize_t channel_write(struct file *file, const char __user *user_buffer, size_t size, loff_t * offset)
{
	int ret = 0;
    struct peer *peer = (struct peer*) file->private_data;
    ssize_t len = WRITE_DATA_SIZE - *offset;
	unsigned long flags;
	__poll_t events;

    if (len <= 0)
        return 0;

    /* read data from user buffer to my_data->buffer */
    ret = copy_from_user(peer + *offset, user_buffer, len);
    if (ret) {
		return -EFAULT;
	}
    *offset += len;

	pr_info("[CH] %s peer's id: %d, eventfd: %d len: %d\n", __func__, peer->id, peer->eventfd, len);
	spin_lock_irqsave(&drv_priv.lock, flags);
	if (drv_priv.id < 0 || !drv_priv.efd_ctx) {
		struct fd f;

		pr_info("[CH] Get Host Channel's peer id: %d, eventfd: %d\n", peer->id, peer->eventfd);
		drv_priv.id = peer->id;
		f = fdget(peer->eventfd);
		if (!f.file) {
			pr_err("[CH] %s fdget failed", __func__);
			goto out;
		}

		drv_priv.efd_ctx = eventfd_ctx_fileget(f.file);
		if (IS_ERR(drv_priv.efd_ctx)) {
			pr_err("[CH] %s eventfd_ctx_fileget failed", __func__);
			fdput(f);
			goto out;
		}

		/*
		 * Check if there was an event already pending on the eventfd before we registered.
		 * This registration should be done before running any realm.
		 * Therefore, if there is any pending event, it is abnormal.
		 */
		events = vfs_poll(f.file, &drv_priv.pt);
		if (events & EPOLLIN) {
			pr_err("[CH] %s pending event is not allowed when registering eventfd", __func__);
		}
	} else if (is_new_peer(&drv_priv, peer->id)) {
		struct eventfd_ctx *peer_efd_ctx = eventfd_ctx_fdget(peer->eventfd);
		if (IS_ERR(peer_efd_ctx)) {
			pr_err("[CH] %s eventfd_ctx_fdget failed 0x%p", __func__, peer_efd_ctx);
			goto out;
		}
		peer->efd_ctx = peer_efd_ctx;
		ret = push_back(&drv_priv, *peer);
		if (ret) {
			pr_err("[CH] failed to push_back a new peer. id: %d, eventfd: %d", peer->id, peer->eventfd);
		}
	} else {
		pr_err("[CH] The peer already exists: id: %d, eventfd: %d", peer->id, peer->eventfd);
	}
out:
	spin_unlock_irqrestore(&drv_priv.lock, flags);
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
	drv_priv.id = -1;
	drv_priv.is_active = true;

	spin_lock_init(&drv_priv.lock);
	init_waitqueue_func_entry(&drv_priv.wait, ch_wakeup);
	init_poll_funcptr(&drv_priv.pt, ch_ptable_queue_proc);

	INIT_WORK(&drv_priv.inject, ch_inject);

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
