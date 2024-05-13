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
#define LOG_ON_DEBUG

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

                printf("[vm] tx write data-%d %d: ", i, iov[i].iov_len);
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
