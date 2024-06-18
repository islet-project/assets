#include "kvm/virtio-pci-dev.h"
#include "kvm/virtio-net.h"
#include "kvm/virtio.h"
#include "kvm/mutex.h"
#include "kvm/util.h"
#include "kvm/kvm.h"
#include "kvm/irq.h"
#include "kvm/uip.h"
#include "kvm/guest_compat.h"
#include "kvm/iovec.h"
#include "kvm/strbuf.h"

#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <linux/if_tun.h>
#include <linux/types.h>

#include <arpa/inet.h>
#include <net/if.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <pthread.h>

// =========================================================
// log
#define LOG_ON_ERROR
//#define LOG_ON_DEBUG

#ifdef LOG_ON_DEBUG
#define LOG_DEBUG(...) \
do { \
	printf(__VA_ARGS__); \
} while(0)
#else
#define LOG_DEBUG(...) do {} while (0)
#endif

#ifdef LOG_ON_ERROR
#define LOG_ERROR(...) \
do { \
	printf(__VA_ARGS__); \
} while(0)
#else
#define LOG_ERROR(...) do {} while (0)
#endif

#define SLEEP_SEC (1)
// =========================================================

#define VIRTIO_NET_QUEUE_SIZE		256
#define VIRTIO_NET_NUM_QUEUES		8

#define VRING_SIZE (16 * 1024 * 1024)
#define VQ_ELEM_SIZE (8 * 1024)

extern char vring_shared[VRING_SIZE];
extern unsigned long base_ipa_addr;
extern unsigned long base_ipa_elem_addr;

// ------------------------------------------------------------------------------------
struct net_tx {
	unsigned int out;
	struct iovec iovs[];
};
struct net_rx {
	unsigned int in_cnt;
	struct iovec iovs[];
};

#define CLOAK_VQ_HOST_9P (0x88400000 + (14 * 1024 * 1024))
#define CLOAK_VQ_HOST_NET_TX (0x88400000 + (18 * 1024 * 1024))
#define CLOAK_VQ_HOST_NET_RX (0x88400000 + (22 * 1024 * 1024))

extern void *get_host_addr_from_offset(struct kvm *kvm, u64 offset);
static unsigned long net_tx_control_addr = 0;
static unsigned long net_tx_data_addr = 0;
static unsigned long net_rx_control_addr = 0;
static unsigned long net_rx_data_addr = 0;
int cloak_tap_fd_tx = -1;

static inline int cloak_tap_ops_tx(struct iovec *iov, u16 out)
{
    if (cloak_tap_fd_tx == -1) {
        cloak_tap_fd_tx = open("/dev/net/tun", O_RDWR);
        if (cloak_tap_fd_tx < 0) {
            printf("tun device open error\n");
        } else {
            printf("kvm-cpu: tap_fd_tx: %d\n", cloak_tap_fd_tx);
        }
    }
	return writev(cloak_tap_fd_tx, iov, out);
}

static void net_translate_addr_space_host(struct kvm *kvm, struct net_tx *tx)
{
	for (unsigned i=0; i<tx->out; i++) {
		unsigned long offset = (unsigned long)(tx->iovs[i].iov_base);
		unsigned long new_addr = (unsigned long)get_host_addr_from_offset(kvm, offset);

		LOG_DEBUG("[host-out-%d] offset: %lx, new_addr: %lx, len: %lx\n", i, offset, new_addr, tx->iovs[i].iov_len);
		tx->iovs[i].iov_base = (void *)new_addr;
	}
}

void run_net_rx_write_num_buffers(struct kvm *kvm, unsigned long iov_base, u16 num_buffers)
{
    unsigned char *ptr;

    if (net_rx_control_addr == 0) {
		net_rx_control_addr = (unsigned long)get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_NET_RX);
		if (net_rx_control_addr == 0) {
			LOG_ERROR("net_rx_control_addr: control error..\n");
            return;
		}
	}
	LOG_DEBUG("net_rx_control_addr: %lx\n", net_rx_control_addr);

    ptr = (unsigned char *)net_rx_control_addr;

    // write iov_base and num_buffers
    memcpy(ptr, &iov_base, sizeof(iov_base));
    ptr += sizeof(iov_base);

    memcpy(ptr, &num_buffers, sizeof(num_buffers));
    ptr += sizeof(num_buffers);
}

void run_net_rx_memcpy_to_iovec(struct kvm *kvm, struct iovec *iov, unsigned char *buf, size_t len)
{
    struct iovec *iiov = iov;
	unsigned int llen = len;
    unsigned int in_cnt = 0;
	unsigned char *data = buf;
    unsigned char *ptr;

    if (net_rx_control_addr == 0) {
		net_rx_control_addr = (unsigned long)get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_NET_RX);
		if (net_rx_control_addr == 0) {
			LOG_ERROR("net_rx_control_addr: control error..\n");
			return;
		}
	}
	LOG_DEBUG("net_rx_control_addr: %lx, %d\n", net_rx_control_addr, llen);
    ptr = (unsigned char *)net_rx_control_addr;

    // 1. write total len
    memcpy(ptr, &llen, sizeof(llen));
    ptr += sizeof(llen);

    // 2. iterate iovs
    while (llen > 0) {
        LOG_DEBUG("net_rx: iov_len: %d\n", iiov->iov_len);

        if (iiov->iov_len) {
            int copy = min_t(unsigned int, iiov->iov_len, llen);

            // write iov first
            memcpy(ptr, iiov, sizeof(struct iovec));
            ptr += sizeof(struct iovec);

            // write data next
            memcpy(ptr, data, copy);
            ptr += copy;

            data += copy;
            llen -= copy;
            iiov->iov_len -= copy;
            iiov->iov_base += copy;
        }
        iiov++;
        in_cnt++;
    }

    LOG_DEBUG("run_net_rx_memcpy_to_iovec done\n");
}

extern void *get_shm(void);
int run_net_tx_operation_in_host(struct kvm *kvm)
{
	int len;
    struct net_tx *tx;
    unsigned char *shm;
    unsigned long offset, new_addr;

	if (net_tx_control_addr == 0) {
		net_tx_control_addr = get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_NET_TX); // 18 mb
		if (net_tx_control_addr == 0) {
			LOG_ERROR("net_tx_control_addr: control error..\n");
			return 0;
		}
	}
	LOG_DEBUG("net_tx_control_addr: %lx\n", net_tx_control_addr);

	// 0. address translation
	tx = (struct net_tx *)net_tx_control_addr;

    // 1. copy to shm instead of doing tap-tx-operation
    shm = get_shm();
    shm += (1 * 1024 * 1024);

    LOG_DEBUG("tx_operation, out_cnt: %d\n", tx->out);

    // out, iov-data, ...
    memcpy(shm, &(tx->out), sizeof(tx->out));
    shm += sizeof(tx->out);

    for (unsigned int i=0; i<tx->out; i++) {
        memcpy(shm, &(tx->iovs[i]), sizeof(struct iovec));
        shm += sizeof(struct iovec);

        LOG_DEBUG("tx_operation, iov_base: %lx, iov_len: %d\n", tx->iovs[i].iov_base, tx->iovs[i].iov_len);
        offset = (unsigned long)(tx->iovs[i].iov_base);
		new_addr = (unsigned long)get_host_addr_from_offset(kvm, offset);

        memcpy(shm, (unsigned char *)new_addr, tx->iovs[i].iov_len);
        shm += tx->iovs[i].iov_len;
    }
    return len;
}
// ------------------------------------------------------------------------------------

static bool read_from_vring(void)
{
	ssize_t res;
	int fd = open("/dev/rsi", O_RDWR);
	if (fd < 0) {
		LOG_ERROR("rsi open error\n");
		return false;
	}

	res = read(fd, vring_shared, sizeof(vring_shared));
	if (res != sizeof(vring_shared)) {
		LOG_ERROR("vring_shared read error\n");
		close(fd);
		return false;
	}
	close(fd);
    return true;
}

static bool write_to_vring(void)
{
    ssize_t res;
	int fd = open("/dev/rsi", O_RDWR);
	if (fd < 0) {
		LOG_ERROR("rsi open error\n");
		return false;
	}

	res = write(fd, vring_shared, sizeof(vring_shared));
	if (res != sizeof(vring_shared)) {
		LOG_ERROR("vring_shared write error\n");
		close(fd);
		return false;
	}
	close(fd);
	return true;
}

static bool translate_net_addr_space(struct iovec *iov, u16 in, u16 out)
{
    // address translation
    if (in > 0) {
        for (unsigned i=0; i<in; i++) {
            unsigned long orig = (unsigned long)(iov[i].iov_base);
            unsigned long offset = orig - base_ipa_addr;
            unsigned long new_addr = (unsigned long)vring_shared + offset;

            /*
            if (offset >= VRING_SIZE) {
                LOG_ERROR("larger than VRING_SIZE!!!: orig: %lx, offset: %lx\n", orig, offset);
                exit(-1);
            } */
            iov[i].iov_base = (void *)new_addr;
        }
    }
    if (out > 0) {
        for (unsigned i=0; i<out; i++) {
            unsigned long orig = (unsigned long)(iov[i].iov_base);
            unsigned long offset = orig - base_ipa_addr;
            unsigned long new_addr = (unsigned long)vring_shared + offset;

            //printf("iov_base: %lx, orig: %lx, base_ipa: %lx, offset: %lx, size: %lx\n", (unsigned long)(iov[i].iov_base), orig, base_ipa_addr, orig - base_ipa_addr, VRING_SIZE);

            /*
            if (offset >= (unsigned long)VRING_SIZE) {
                LOG_ERROR("larger than VRING_SIZE!!!: orig: %lx, offset: %lx\n", orig, offset);
                exit(-1);
            } */
            iov[i].iov_base = (void *)new_addr;
        }
    }

	return true;
}

void* run_vm_rx_thread(void* arg) {
    printf("run_vm_rx_thread start\n");

    while (true) {
        FILE *fp = NULL, *fpd = NULL;
        struct iovec iov;
        int len = 0;

        // 1. read iov from file
        fp = fopen("/shared/net_rx.bin", "rb");
        fpd = fopen("/shared/net_rx_data.bin", "rb");
        if (fp && fpd) {
            unsigned int llen = 0;
            struct iovec iiov;

            // sync first
            read_from_vring();

            fread((unsigned int *)&llen, sizeof(unsigned int), 1, fp);
            while (llen > 0) {
                fread((struct iovec *)&iiov, sizeof(struct iovec), 1, fp);
                translate_net_addr_space(&iiov, 1, 0);

                if (iiov.iov_len) {
                    int copy = min_t(unsigned int, iiov.iov_len, llen);
                    fread((unsigned char *)iiov.iov_base, sizeof(unsigned char), copy, fpd);

                    printf("[vm] rx read data %d: ", copy);
                    for (unsigned i=0; i<copy; i++) {
                        printf("%02x ", ((unsigned char *)iiov.iov_base)[i]);
                        if (i >= 64)
                            break;
                    }
                    printf("\n");

                    llen -= copy;
                    iiov.iov_len -= copy;
                    iiov.iov_base += copy;
                }
            }

            fclose(fp);
            fclose(fpd);
        } else {
            sleep(SLEEP_SEC);
            continue;
        }
        printf("read net_rx.bin success\n");

        // 2. do something with data (e.g., filtering or encryption)
        // ... todo ...

        write_to_vring();
        remove("/shared/net_rx_data.bin");
        remove("/shared/net_rx.bin");
        sleep(SLEEP_SEC);
    }
    return NULL;
}

void *run_vm_rx_num_buf_thread(void *arg) {
    printf("run_vm_rx_num_buf_threads start\n");

    while (true) {
        FILE *fp = NULL;
        unsigned long iov_base, new_addr;
        u16 num_buffers;
        struct virtio_net_hdr_mrg_rxbuf *hdr;

        // 1. read
        fp = fopen("/shared/net_rx_num_buffers.bin", "rb");
        if (fp) {
            fread((unsigned long *)&iov_base, sizeof(unsigned long), 1, fp);
            fread((u16 *)&num_buffers, sizeof(u16), 1, fp);
            fclose(fp);
            printf("net_rx_num_buffers.bin read success: %lx, %d\n", iov_base, num_buffers);
        } else {
            sleep(SLEEP_SEC);
            continue;
        }

        // 2. do something with data
        new_addr = (unsigned long)vring_shared + (iov_base - base_ipa_addr);
        hdr = (struct virtio_net_hdr_mrg_rxbuf *)new_addr;
        hdr->num_buffers = num_buffers;

        write_to_vring();
        remove("/shared/net_rx_num_buffers.bin");
        sleep(SLEEP_SEC);
    }
    return NULL;
}

void* run_vm_tx_thread(void* arg) {
    printf("run_vm_tx_thread start\n");

    while (true) {
        FILE *fp = NULL;
        struct iovec iov[VIRTIO_NET_QUEUE_SIZE];
        u16 out;
        int len = 0;

        // 1. read net_tx.bin
        fp = fopen("/shared/net_tx.bin", "rb");
        if (fp) {
            fread((struct iovec *)iov, sizeof(struct iovec), VIRTIO_NET_QUEUE_SIZE, fp);
            fread((u16 *)&out, sizeof(u16), 1, fp);
            fclose(fp);
        } else {
            sleep(SLEEP_SEC);
            continue;
        }
        printf("read net_tx.bin success: out: %d\n", out);

        read_from_vring();
        translate_net_addr_space(iov, 0, out);

        // 2. do something with data.. <todo>

        // 3. data write
        // -- data layout: iov0_data - iov1_data ...
        fp = fopen("/shared/net_tx_data.bin", "wb");
        if (fp) {
            for (unsigned i = 0; i < out; i++) {
                fwrite((unsigned char *)iov[i].iov_base, sizeof(unsigned char), iov[i].iov_len, fp);

                //printf("[vm] tx write data-%d %d: ", i, iov[i].iov_len);
                for (unsigned j=0; j<iov[i].iov_len; j++) {
                    printf("%02x ", ((unsigned char *)iov[i].iov_base)[j]);
                    if (j >= 64)
                        break;
                }
                printf("\n");

                len += iov[i].iov_len;
            }
            fclose(fp);
        }
        write_to_vring();

        // 4. remove
        remove("/shared/net_tx.bin");
        sleep(SLEEP_SEC);
    }
    return NULL;
}
