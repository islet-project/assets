#include "kvm/virtio.h"
#include "kvm/mutex.h"
#include "kvm/util.h"
#include "kvm/kvm.h"
#include "kvm/iovec.h"

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

#define CLOAK_VQ_HOST_BLK (0x88400000 + (26 * 1024 * 1024))
#define CLOAK_VQ_HOST_BLK_IN (0x88400000 + (30 * 1024 * 1024))
#define VIRTIO_START (0x88400000)

static unsigned long blk_control_addr = 0;
static unsigned long blk_in_control_addr = 0;

extern void *get_host_addr_from_offset(struct kvm *kvm, u64 offset);
extern void *get_shm(void);

struct block_req_host {
	unsigned int blk_type;
	unsigned int cnt;
	unsigned long sector;
	unsigned long status;
	struct iovec iovs[];
    // data copy--
};

void run_blk_operation_in_host(struct kvm *kvm)
{
    struct block_req_host *req_host = NULL, *dst_req = NULL;
    unsigned char *shm = get_shm();
    unsigned char *data_ptr = NULL;
    
    shm += (2 * 1024 * 1024);
    dst_req = (struct block_req_host *)shm;

    // read BlockReqHost
    if (blk_control_addr == 0) {
        blk_control_addr = (unsigned long)get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_BLK);
        if (blk_control_addr == 0) {
            LOG_ERROR("blk_control_addr: control error..\n");
            return;
        }
    }
    LOG_DEBUG("blk_control_addr: %lx\n", blk_control_addr);

    req_host = (struct block_req_host *)blk_control_addr;
    LOG_DEBUG("blk, iovcount %d, type %d, sector %d\n", req_host->cnt, req_host->blk_type, req_host->sector);

    dst_req->cnt = req_host->cnt;
    dst_req->blk_type = req_host->blk_type;
    dst_req->sector = req_host->sector;
    dst_req->status = req_host->status;

    // iov copy
    for (unsigned i=0; i<dst_req->cnt; i++) {
        dst_req->iovs[i].iov_base = req_host->iovs[i].iov_base;
        dst_req->iovs[i].iov_len = req_host->iovs[i].iov_len;

        LOG_DEBUG("blk, iov %lx-%lx, %d\n", req_host->iovs[i].iov_base, dst_req->iovs[i].iov_base, dst_req->iovs[i].iov_len);
    }

    // data copy
    data_ptr = (unsigned char *)&dst_req->iovs[req_host->cnt];
    for (unsigned i=0; i<dst_req->cnt; i++) {
        unsigned long new_addr = (unsigned long)get_host_addr_from_offset(kvm, dst_req->iovs[i].iov_base);

        memcpy(data_ptr, (unsigned char *)(new_addr), dst_req->iovs[i].iov_len);
        data_ptr += dst_req->iovs[i].iov_len;
    }
}

void run_blk_in_operation_in_host(struct kvm *kvm)
{
    struct block_req_host *req_host = NULL, *dst_req = NULL;
    unsigned char *shm = get_shm();
    unsigned char *dst_ptr = NULL;
    unsigned char *src_ptr = NULL;
    
    shm += (2 * 1024 * 1024);
    dst_req = (struct block_req_host *)shm;

    // read BlockReqHost
    if (blk_in_control_addr == 0) {
        blk_in_control_addr = (unsigned long)get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_BLK_IN);
        if (blk_in_control_addr == 0) {
            LOG_ERROR("blk_in_control_addr: control error..\n");
            return;
        }
    }
    LOG_DEBUG("blk_in_control_addr: %lx\n", blk_in_control_addr);
    LOG_DEBUG("dst_req->cnt: %d, dst_req->status: %lx\n", dst_req->cnt, dst_req->status);

    req_host = (struct block_req_host *)blk_in_control_addr;

    // copy metadata
    req_host->blk_type = dst_req->blk_type;
    req_host->cnt = dst_req->cnt;
    req_host->sector = dst_req->sector;
    req_host->status = dst_req->status;

    // copy iovs & data
    //dst_ptr = &req_host->iovs[req_host->cnt]; // dst: host_shared
    src_ptr = &dst_req->iovs[req_host->cnt];  // source: shm

    for (unsigned i=0; i<req_host->cnt; i++) {
        // copy iov
        req_host->iovs[i].iov_base = dst_req->iovs[i].iov_base;
        req_host->iovs[i].iov_len = dst_req->iovs[i].iov_len;

        // copy data
        dst_ptr = (unsigned char *)get_host_addr_from_offset(kvm, req_host->iovs[i].iov_base);
        memcpy(dst_ptr, src_ptr, dst_req->iovs[i].iov_len);
        src_ptr += dst_req->iovs[i].iov_len;
    }
}