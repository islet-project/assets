/*
 * Copyright (c) 2023, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

//#include "val_realm_framework.h"
//#include "val_irq.h"
//#include "val_realm_rsi.h"
//#include "val.h"

#include "gateway.h"
#include "val_realm_framework.h"
#include "val_realm_memory.h"

#define CLOAK_MSG_TYPE_P9 (2)
#define CLOAK_MSG_TYPE_NET_TX (3)
#define CLOAK_MSG_TYPE_NET_RX (4)
#define CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS (5)

#define CLOAK_MSG_TYPE_P9_RESP (12)
#define CLOAK_MSG_TYPE_NET_RX_RESP (14)
#define CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS_RESP (15)

#define RSI_RIPAS_EMPTY (0)
#define RSI_RIPAS_RAM (1)
#define PROT_NS_SHARED (0x100000000) // for which ips_bits is 33

// -------------------- smc call -------------------------
typedef struct {
    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t x9;
    uint64_t x10;
} smc_param_ts;

smc_param_ts smc_call(uint64_t x0, uint64_t x1, uint64_t x2,
                                uint64_t x3, uint64_t x4, uint64_t x5,
                                uint64_t x6, uint64_t x7, uint64_t x8,
                                uint64_t x9, uint64_t x10)
{
    smc_param_ts args;

    args.x0 = x0;
    args.x1 = x1;
    args.x2 = x2;
    args.x3 = x3;
    args.x4 = x4;
    args.x5 = x5;
    args.x6 = x6;
    args.x7 = x7;
    args.x8 = x8;
    args.x9 = x9;
    args.x10 = x10;
    val_smc_call_asm(&args);
    return args;
}
// -------------------------------------------------------

uint64_t val_realm_get_secondary_cpu_entry(void)
{
    return (uint64_t)&acs_realm_entry;
}

static inline unsigned long rsi_set_addr_range_state(unsigned long start,
                             unsigned long end,
                             unsigned long ripas_state,
                             unsigned long *top)
{
    smc_param_ts res = smc_call(RSI_IPA_STATE_SET, start, end - start, ripas_state, 0, 0, 0, 0, 0, 0, 0);
    *top = res.x1;
    return res.x0;
}

static inline void set_memory_range(unsigned long start, unsigned long end, unsigned long ripas_state)
{
    unsigned long top;

    while (start != end) {
        rsi_set_addr_range_state(start, end, ripas_state, &top);
        start = top;
    }
}

static inline void set_memory_range_shared(unsigned long start, unsigned long end)
{
    set_memory_range(start, end, RSI_RIPAS_EMPTY);
}

static void cloak_set_memory_host_shared(unsigned long start, unsigned long end)
{
    // start, end: physical address
    set_memory_range_shared(start, end);

    // already mapped in the form of identity mapping!
}
static void cloak_set_memory_realm(unsigned long start, unsigned long end)
{
    set_memory_range(start, end, RSI_RIPAS_RAM);
}

// ---------------------- CVM GW stuff ------------------------------------
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#define VIRTQUEUE_NUM       128

struct iovec {
    unsigned long iov_base;
    unsigned long iov_len;
};
struct p9_pdu {
    u32         queue_head;
    size_t          read_offset;
    size_t          write_offset;
    u16         out_iov_cnt;
    u16         in_iov_cnt;
    struct iovec        in_iov[VIRTQUEUE_NUM];
    struct iovec        out_iov[VIRTQUEUE_NUM];
};

struct host_call_arg {
	u16 imm;
	unsigned long gprs[7];
};

struct host_call_arg host_call_mem __attribute__((aligned(PAGE_SIZE)));
char __attribute__((aligned(PAGE_SIZE))) cvm_vq_ctrl[2 * 1024 * 1024] = {0,}; // 2mb
char __attribute__((aligned(PAGE_SIZE))) cvm_vq_data[20 * 1024 * 1024] = {0,}; // 20mb
char *cloak_vq_data = NULL;

#define CLOAK_HOST_CALL (799)
#define FIRST_CLOAK_OUTLEN (999999)
#define P9_ADDR_HOST_VQ_CTRL_9P (0x188400000 + (14 * 1024 * 1024))
#define P9_ADDR_HOST_VQ_CTRL_NET_TX (0x188400000 + (18 * 1024 * 1024))
#define P9_ADDR_HOST_VQ_CTRL_NET_RX (0x188400000 + (22 * 1024 * 1024))
#define P9_ADDR_HOST_VQ_DATA (0x188400000)
#define VIRTIO_START (0x88400000)
#define IPA_OFFSET (0x100000000)

struct net_tx_cloak {
	unsigned int out;
	struct iovec iovs[];
};
struct net_rx_cloak {
	unsigned int in_cnt;
	struct iovec iovs[];
};
#define CLOAK_VQ_DESC_9P (cvm_vq_ctrl)
#define CLOAK_VQ_DESC_NET_TX (cvm_vq_ctrl + (1 * 1024 * 1024))
#define CLOAK_VQ_DESC_NET_RX (cvm_vq_ctrl + (1 * 1024 * 1024) + (512 * 1024))

static int do_cloak_host_call(unsigned long outlen, unsigned long *type)
{
    smc_param_ts rets;

	pal_memset(&host_call_mem, 0, sizeof(host_call_mem));
	host_call_mem.imm = CLOAK_HOST_CALL;
	host_call_mem.gprs[0] = outlen;

	rets = smc_call(RSI_HOST_CALL, (uint64_t)&host_call_mem, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    (void)rets;

    //val_realm_cloak_print_call("cloak_host_call_response", host_call_mem.gprs[6], host_call_mem.gprs[6]);
    *type = host_call_mem.gprs[6];

    return 0;  // do not use return value as of now
}

static int do_cloak_create(unsigned long id, unsigned long ipa, unsigned long size)
{
    smc_call(RSI_CHANNEL_CREATE, id, ipa, size, 0, 0, 0, 0, 0, 0, 0);
    return 0;
}

static void copy_iovs(struct iovec *iovs, unsigned int cnt, bool to_host_shared)
{
    // to_host_shared == true: src-cvm_shared, dst-host_shared
    // to_host_shared == false: src-host_shared, dst-cvm_shared
    unsigned long offset, len, src_addr, dst_addr;

    for (unsigned int i = 0; i < cnt; i++) {
        offset = (unsigned long)(iovs[i].iov_base) - VIRTIO_START;
		len = iovs[i].iov_len;
        if (to_host_shared) {
            src_addr = (unsigned long)cloak_vq_data + offset;
            dst_addr = (unsigned long)(iovs[i].iov_base) + IPA_OFFSET;
        } else {
            dst_addr = (unsigned long)cloak_vq_data + offset;
            src_addr = (unsigned long)(iovs[i].iov_base) + IPA_OFFSET;
        }

        pal_memcpy((char *)dst_addr, (char *)src_addr, len);
    }
}

// P9
static bool do_copy_p9pdu_request(struct p9_pdu *p9pdu)
{
	struct p9_pdu *virt_iov = (struct p9_pdu *)P9_ADDR_HOST_VQ_CTRL_9P;

    //val_realm_cloak_print_call("before pal_memcpy", 0, 0);

	// 1. memcpy iov first
	pal_memcpy(virt_iov, p9pdu, sizeof(struct p9_pdu));

    /*
    do {
        val_realm_cloak_print_call("after pal_memcpy", virt_iov->in_iov_cnt, virt_iov->out_iov_cnt);
    } while (0); */

	// 2. copy datap
    copy_iovs(p9pdu->in_iov, p9pdu->in_iov_cnt, true);
    copy_iovs(p9pdu->out_iov, p9pdu->out_iov_cnt, true);
	return true;
}

static bool do_copy_p9pdu_response(struct p9_pdu *p9pdu)
{
	// 1. copy data
	// src: Host_Shared, dst: CVM_Shared
    copy_iovs(p9pdu->in_iov, p9pdu->in_iov_cnt, false);
    copy_iovs(p9pdu->out_iov, p9pdu->out_iov_cnt, false);
	return true;
}

// Net TX
static bool do_copy_net_tx_request(void)
{
    struct net_tx_cloak *net_tx = (struct net_tx_cloak *)CLOAK_VQ_DESC_NET_TX;
    struct net_tx_cloak *tx = (struct net_tx_cloak *)P9_ADDR_HOST_VQ_CTRL_NET_TX;

    // 1. copy iov
    // net_tx: cvm_shared, tx: host_shared
    tx->out = net_tx->out;
    for (unsigned int i=0; i<tx->out; i++) {
        pal_memcpy((unsigned char *)(&(tx->iovs[i])), (unsigned char *)(&(net_tx->iovs[i])), sizeof(struct iovec));
    }

    // do something for security here!
    
    // 2. copy data
    copy_iovs(&net_tx->iovs[0], net_tx->out, true);
    return true;
}

// Net RX main data
static bool do_copy_net_rx_response(void)
{
    unsigned int llen = 0;
    struct iovec iiov;
    unsigned char *ptr = (unsigned char *)P9_ADDR_HOST_VQ_CTRL_NET_RX;
    unsigned long offset, dst_addr;
    struct net_rx_cloak *dst_rx = (struct net_rx_cloak *)P9_ADDR_HOST_VQ_CTRL_NET_RX;
    unsigned int in_cnt = 0;

    // 1. read total_len
    pal_memcpy(&llen, ptr, sizeof(llen));
    ptr += sizeof(unsigned int);

    // 2. iterate iovs
    while (llen > 0) {
        // read iovec
        pal_memcpy(&iiov, ptr, sizeof(struct iovec));
        ptr += sizeof(struct iovec);

        // translate addr
        offset = (unsigned long)(iiov.iov_base) - VIRTIO_START;
        dst_addr = (unsigned long)cloak_vq_data + offset;

        if (iiov.iov_len) {
            int copy = (iiov.iov_len > llen) ? (llen) : (iiov.iov_len);

            pal_memcpy((unsigned char *)dst_addr, ptr, copy);
            ptr += copy;

            llen -= copy;
            iiov.iov_len -= copy;
            iiov.iov_base += copy;
        }

        in_cnt++;
    }
    return true;
}

// Net RX num_buffers
struct virtio_net_hdr {
    /* See VIRTIO_NET_HDR_F_* */
    u8 flags;
    /* See VIRTIO_NET_HDR_GSO_* */
    u8 gso_type;
    u16 hdr_len;     /* Ethernet + IP + tcp/udp hdrs */
    u16 gso_size;        /* Bytes to append to hdr_len per frame */
    u16 csum_start;  /* Position to start checksumming from */
    u16 csum_offset; /* Offset after that to place checksum */
};
struct virtio_net_hdr_mrg_rxbuf {
    struct virtio_net_hdr hdr;
    u16 num_buffers; /* Number of merged rx buffers */
};
static bool do_copy_net_rx_num_buffers(void)
{
    unsigned char *ptr = (unsigned char *)P9_ADDR_HOST_VQ_CTRL_NET_RX;
    struct virtio_net_hdr_mrg_rxbuf *hdr = NULL;
    unsigned long iov_base, new_addr;
    u16 num_buffers;

    pal_memcpy(&iov_base, ptr, sizeof(iov_base));
    ptr += sizeof(iov_base);
        
    pal_memcpy(&num_buffers, ptr, sizeof(num_buffers));
    ptr += sizeof(num_buffers);

    new_addr = (unsigned long)cloak_vq_data + (iov_base - VIRTIO_START);
    hdr = (struct virtio_net_hdr_mrg_rxbuf *)new_addr;
    hdr->num_buffers = num_buffers;
    return true;
}

static unsigned long align_to_2mb(unsigned long value) {
    unsigned long align_min = 2 * 1024 * 1024;
    unsigned long remainder = value % align_min;
    unsigned long aligned_value = value + (align_min - remainder);
    return aligned_value;
}

static void create_shared_mem(void)
{
    cloak_vq_data = (char *)cvm_vq_data;
	cloak_vq_data = (char *)align_to_2mb((unsigned long)cloak_vq_data);

    do_cloak_create(0, (unsigned long)cloak_vq_data, 16 * 1024 * 1024);
    do_cloak_create(1, (unsigned long)cvm_vq_ctrl, 2 * 1024 * 1024);
}

static void p9_vm_thread(void)
{
    unsigned long outlen = FIRST_CLOAK_OUTLEN;
    unsigned long msg_type;
    struct p9_pdu p9pdu;
    //struct net_tx_cloak tx;

    create_shared_mem();
    val_realm_cloak_print_call("p9_vm_thread start2", 0, 0);

    /*
    do {
        char *virt_iov = (char *)P9_ADDR_HOST_VQ_CTRL;
        virt_iov[0] = 0x22;
        virt_iov[2] = 0x77;
        val_realm_cloak_print_call("vq_ctrl test", virt_iov[0], virt_iov[2]);
    } while (0); */

    // main_loop
    while (true) {
        // 1. wait for a request
        do_cloak_host_call(outlen, &msg_type);

        // 2. handle request
        if (msg_type == CLOAK_MSG_TYPE_P9) {
            pal_memcpy(&p9pdu, CLOAK_VQ_DESC_9P, sizeof(p9pdu));
            do_copy_p9pdu_request(&p9pdu);
        }
        else if (msg_type == CLOAK_MSG_TYPE_P9_RESP) {
            do_copy_p9pdu_response(&p9pdu);
        }
        else if (msg_type == CLOAK_MSG_TYPE_NET_TX) {
            //pal_memcpy(&tx, CLOAK_VQ_DESC_NET_TX, sizeof(tx));
            do_copy_net_tx_request();
        }
        else if (msg_type == CLOAK_MSG_TYPE_NET_RX) {
            // do nothing;
            val_realm_cloak_print_call("NET_RX called!", 0, 0);
        }
        else if (msg_type == CLOAK_MSG_TYPE_NET_RX_RESP) {
            do_copy_net_rx_response();
        }
        else if (msg_type == CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS) {
            // do nothing;
            val_realm_cloak_print_call("NET_RX_NUM_BUFFERS called!", 0, 0);
        }
        else if (msg_type == CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS_RESP) {
            do_copy_net_rx_num_buffers();
        } 
        else {
            val_realm_cloak_print_call("unsupported msg type", msg_type, 0);
        }

        outlen = 0;
    }
}
// -----------------------------------------------------------------------

/**
 *   @brief    C entry function for endpoint
 *   @param    primary_cpu_boot     - Boolean value for primary cpu boot
 *   @return   void (Never returns)
**/
void val_realm_main(bool primary_cpu_boot)
{
    unsigned long shared_ipa_addr_start[8] = {
        0x88400000,
        0x8c400000,
        0x8c420000,
        0x8c440000,
        0x8c460000,
        0x8c464000,
        0x8c468000,
        0x8c46c000,
    };
    unsigned long shared_ipa_addr_end[8] = {
        0x8c400000,
        0x8c420000,
        0x8c440000,
        0x8c460000,
        0x8c463000,
        0x8c467000,
        0x8c46b000,
        0x8c46e000,
    };

    val_set_running_in_realm_flag();
    val_set_security_state_flag(2);

    uint64_t ipa_width = val_realm_get_ipa_width();

    xlat_ctx_t  *realm_xlat_ctx = val_realm_get_xlat_ctx();

    if (primary_cpu_boot == true)
    {
        /* Add realm region into TT data structure */
        val_realm_add_mmap();

        val_realm_update_xlat_ctx_ias_oas(((1UL << ipa_width) - 1), ((1UL << ipa_width) - 1));
        /* Write page tables */
        val_setup_mmu(realm_xlat_ctx);
    }

    /* Enable Stage-1 MMU */
    val_enable_mmu(realm_xlat_ctx);

    val_irq_setup();
    // [JB] Cloak: main logic here!
    do {
        //char *shared_addr = (char *)(0x188400000 + (14 * 1024 * 1024)); // --> this memory is mapped?

        // RSI_IPA_STATE_SET!
        // 1. realm pas
        cloak_set_memory_realm(0x80000000, 0x90000000);

        // 2. ns pas
        for (unsigned i=0; i<1; i++) {  // first memory only!
            cloak_set_memory_host_shared(shared_ipa_addr_start[i], shared_ipa_addr_end[i]);
        }

        //val_realm_cloak_print_call("shared0", shared_addr[0], shared_addr[2]);
        //val_realm_cloak_print_call("jinbum2", 0x1, 0x12);
        p9_vm_thread();
    } while (0);
    
    //val_realm_rsi_host_call(78);

    /* Ready to run test regression */
    /*
    val_realm_test_dispatch();
    LOG(ALWAYS, "REALM : Entering standby.. \n", 0, 0);
    pal_terminate_simulation(); */
}
