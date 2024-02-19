#include "kvm/virtio-console.h"
#include "kvm/virtio-pci-dev.h"
#include "kvm/disk-image.h"
#include "kvm/virtio.h"
#include "kvm/ioport.h"
#include "kvm/util.h"
#include "kvm/term.h"
#include "kvm/mutex.h"
#include "kvm/kvm.h"
#include "kvm/pci.h"
#include "kvm/threadpool.h"
#include "kvm/irq.h"
#include "kvm/guest_compat.h"

#include <linux/virtio_console.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_blk.h>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define VIRTIO_CONSOLE_QUEUE_SIZE	128
#define VIRTIO_CONSOLE_NUM_QUEUES	2
#define VIRTIO_CONSOLE_RX_QUEUE		0
#define VIRTIO_CONSOLE_TX_QUEUE		1

struct con_dev {
	struct mutex			mutex;

	struct virtio_device		vdev;
	struct virt_queue		vqs[VIRTIO_CONSOLE_NUM_QUEUES];
	struct virtio_console_config	config;
	int				vq_ready;

	struct thread_pool__job		jobs[VIRTIO_CONSOLE_NUM_QUEUES];
};

static struct con_dev cdev = {
	.mutex				= MUTEX_INITIALIZER,
	.vq_ready			= 0,
};

static int compat_id = -1;

/*
 * Interrupts are injected for hvc0 only.
 */
static void virtio_console__inject_interrupt_callback(struct kvm *kvm, void *param)
{
	struct iovec iov[VIRTIO_CONSOLE_QUEUE_SIZE];
	struct virt_queue *vq;
	u16 out, in;
	u16 head;
	int len;

	mutex_lock(&cdev.mutex);

	vq = param;

	if (term_readable(0) && virt_queue__available(vq)) {
		head = virt_queue__get_iov(vq, iov, &out, &in, kvm);
		len = term_getc_iov(kvm, iov, in, 0);
		virt_queue__set_used_elem(vq, head, len);
		cdev.vdev.ops->signal_vq(kvm, &cdev.vdev, vq - cdev.vqs);
	}

	mutex_unlock(&cdev.mutex);
}

void virtio_console__inject_interrupt(struct kvm *kvm)
{
	if (kvm->cfg.active_console != CONSOLE_VIRTIO)
		return;

	mutex_lock(&cdev.mutex);
	if (cdev.vq_ready)
		thread_pool__do_job(&cdev.jobs[VIRTIO_CONSOLE_RX_QUEUE]);
	mutex_unlock(&cdev.mutex);
}

static void virtio_console_handle_callback(struct kvm *kvm, void *param)
{
	struct iovec iov[VIRTIO_CONSOLE_QUEUE_SIZE];
	struct virt_queue *vq;
	u16 out, in;
	u16 head;
	u32 len;

	vq = param;

	/*
	 * The current Linux implementation polls for the buffer
	 * to be used, rather than waiting for an interrupt.
	 * So there is no need to inject an interrupt for the tx path.
	 */

	while (virt_queue__available(vq)) {
		head = virt_queue__get_iov(vq, iov, &out, &in, kvm);
		len = term_putc_iov(iov, out, 0);
		virt_queue__set_used_elem(vq, head, len);
	}

}

static u8 *get_config(struct kvm *kvm, void *dev)
{
	struct con_dev *cdev = dev;

	return ((u8 *)(&cdev->config));
}

static size_t get_config_size(struct kvm *kvm, void *dev)
{
	struct con_dev *cdev = dev;

	return sizeof(cdev->config);
}

static u64 get_host_features(struct kvm *kvm, void *dev)
{
	return 1 << VIRTIO_F_ANY_LAYOUT;
}

static void notify_status(struct kvm *kvm, void *dev, u32 status)
{
	struct con_dev *cdev = dev;
	struct virtio_console_config *conf = &cdev->config;

	if (!(status & VIRTIO__STATUS_CONFIG))
		return;

	conf->cols = virtio_host_to_guest_u16(&cdev->vdev, 80);
	conf->rows = virtio_host_to_guest_u16(&cdev->vdev, 24);
	conf->max_nr_ports = virtio_host_to_guest_u32(&cdev->vdev, 1);
}

static int init_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct virt_queue *queue;

	BUG_ON(vq >= VIRTIO_CONSOLE_NUM_QUEUES);

	compat__remove_message(compat_id);

	queue		= &cdev.vqs[vq];

	virtio_init_device_vq(kvm, &cdev.vdev, queue, VIRTIO_CONSOLE_QUEUE_SIZE);

	if (vq == VIRTIO_CONSOLE_TX_QUEUE) {
		thread_pool__init_job(&cdev.jobs[vq], kvm, virtio_console_handle_callback, queue);
	} else if (vq == VIRTIO_CONSOLE_RX_QUEUE) {
		thread_pool__init_job(&cdev.jobs[vq], kvm, virtio_console__inject_interrupt_callback, queue);
		/* Tell the waiting poll thread that we're ready to go */
		mutex_lock(&cdev.mutex);
		cdev.vq_ready = 1;
		mutex_unlock(&cdev.mutex);
	}
    pr_err("[JB] console init_vq: %lx\n", (unsigned long)queue->vring_addr.pfn);

	return 0;
}

static void exit_vq(struct kvm *kvm, void *dev, u32 vq)
{
	if (vq == VIRTIO_CONSOLE_RX_QUEUE) {
		mutex_lock(&cdev.mutex);
		cdev.vq_ready = 0;
		mutex_unlock(&cdev.mutex);
		thread_pool__cancel_job(&cdev.jobs[vq]);
	} else if (vq == VIRTIO_CONSOLE_TX_QUEUE) {
		thread_pool__cancel_job(&cdev.jobs[vq]);
	}
}

static int notify_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct con_dev *cdev = dev;

	thread_pool__do_job(&cdev->jobs[vq]);

	return 0;
}

static struct virt_queue *get_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct con_dev *cdev = dev;

	return &cdev->vqs[vq];
}

static int get_size_vq(struct kvm *kvm, void *dev, u32 vq)
{
	return VIRTIO_CONSOLE_QUEUE_SIZE;
}

static int set_size_vq(struct kvm *kvm, void *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static unsigned int get_vq_count(struct kvm *kvm, void *dev)
{
	return VIRTIO_CONSOLE_NUM_QUEUES;
}

static struct virtio_ops con_dev_virtio_ops = {
	.get_config		= get_config,
	.get_config_size	= get_config_size,
	.get_host_features	= get_host_features,
	.get_vq_count		= get_vq_count,
	.init_vq		= init_vq,
	.exit_vq		= exit_vq,
	.notify_status		= notify_status,
	.notify_vq		= notify_vq,
	.get_vq			= get_vq,
	.get_size_vq		= get_size_vq,
	.set_size_vq		= set_size_vq,
};

int virtio_console__init(struct kvm *kvm)
{
	int r;

    pr_err("[JB] virtio_console__init start\n");
	if (kvm->cfg.active_console != CONSOLE_VIRTIO)
		return 0;

	r = virtio_init(kvm, &cdev, &cdev.vdev, &con_dev_virtio_ops,
			VIRTIO_DEFAULT_TRANS(kvm), PCI_DEVICE_ID_VIRTIO_CONSOLE,
			VIRTIO_ID_CONSOLE, PCI_CLASS_CONSOLE);
	if (r < 0)
		return r;

	if (compat_id == -1)
		compat_id = virtio_compat_add_message("virtio-console", "CONFIG_VIRTIO_CONSOLE");

    pr_err("[JB] virtio_console__init end\n");
	return 0;
}
virtio_dev_init(virtio_console__init);

int virtio_console__exit(struct kvm *kvm)
{
	return 0;
}
virtio_dev_exit(virtio_console__exit);
