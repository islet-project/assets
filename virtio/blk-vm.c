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

#define CLOAK_VQ_HOST_BLK (0x99600000 + (26 * 1024 * 1024))
#define CLOAK_VQ_HOST_BLK_IN (0x99600000 + (30 * 1024 * 1024))
#define CLOAK_VQ_HOST_BLK_AES_TAG (0x99600000 + (34 * 1024 * 1024))
#define VIRTIO_START (0x99600000)

static unsigned long blk_control_addr = 0;
static unsigned long blk_in_control_addr = 0;
static unsigned long blk_tag_addr = 0;

// aes gcm tag storage
struct blk_aes_tag {
    unsigned char tag[16];
};
static struct blk_aes_tag tag_storage[32000];  // idx=sector, todo: hard-coded value

extern void *get_host_addr_from_offset(struct kvm *kvm, u64 offset);
extern void *get_shm(void);

struct block_req {
    unsigned int out_cnt;
    unsigned int in_cnt;
    struct iovec iovs[];
};

struct block_req_host {
	unsigned int blk_type;
	unsigned int cnt;
	unsigned long sector;
	unsigned long status;
	struct iovec iovs[];
    // data copy--
};

#define TAG_STORAGE_FILE_NAME "/shared/disk.tag"

void load_tag_storage(struct kvm *kvm)
{
    FILE *fp = NULL;

    if (blk_tag_addr != 0) {
        // already loaded
        return;
    } else {
        blk_tag_addr = (unsigned long)get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_BLK_AES_TAG);
        if (blk_tag_addr == 0) {
            LOG_ERROR("blk_tag_addr: error..\n");
            return;
        }
        LOG_ERROR("blk_tag_addr: %lx\n", blk_tag_addr);
        LOG_ERROR("tag_storage size: %d\n", sizeof(tag_storage));
    }
    memset(&tag_storage, 0, sizeof(tag_storage));

    fp = fopen(TAG_STORAGE_FILE_NAME, "rb");
    if (fp) {
        fread((unsigned char *)&tag_storage, sizeof(unsigned char *), sizeof(tag_storage), fp);
        memcpy((unsigned char *)blk_tag_addr, &tag_storage, sizeof(tag_storage));
        fclose(fp);
    } else {
        LOG_ERROR("tag_storage file open error\n");
    }
}

void sync_tag_storage(void)
{
    FILE *fp = NULL;

    if (blk_tag_addr == 0) {
        LOG_ERROR("blk_tag_addr 0, error\n");
        return;
    }

    memcpy(&tag_storage, (unsigned char *)blk_tag_addr, sizeof(tag_storage));

    fp = fopen(TAG_STORAGE_FILE_NAME, "wb");
    if (fp) {
        fwrite((unsigned char *)&tag_storage, sizeof(unsigned char *), sizeof(tag_storage), fp);
        fclose(fp);
    } else {
        LOG_ERROR("tag_storage file open error\n");
    }
}

void send_block_req_to_gw(struct kvm *kvm)
{
    struct block_req *src_req = NULL;
    struct block_req *dst_req = NULL;
    unsigned char *shm = get_shm();

    if (blk_control_addr == 0) {
        blk_control_addr = (unsigned long)get_host_addr_from_offset(kvm, CLOAK_VQ_HOST_BLK);
        if (blk_control_addr == 0) {
            LOG_ERROR("blk_control_addr: control error..\n");
            return;
        }
        load_tag_storage(kvm);
    }
    LOG_DEBUG("blk_control_addr: %lx\n", blk_control_addr);

    // read block_req and pass it on to CVM_GW first
    shm += (2 * 1024 * 1024);
    src_req = (struct block_req *)shm;
    dst_req = (struct block_req *)blk_control_addr;

    dst_req->out_cnt = src_req->out_cnt;
    dst_req->in_cnt = src_req->in_cnt;
    for (unsigned i=0; i<dst_req->out_cnt + dst_req->in_cnt; i++) {
        dst_req->iovs[i].iov_base = src_req->iovs[i].iov_base;
        dst_req->iovs[i].iov_len = src_req->iovs[i].iov_len;
    }
}

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
        load_tag_storage(kvm);
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
        load_tag_storage(kvm);
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
