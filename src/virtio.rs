use crate::rsi::*;
use crate::module::*;
//use crate::aes::*;
use alloc::vec;
//use alloc::vec::Vec;

core::arch::global_asm!(include_str!("memcpy.S"));
extern "C" {
    fn asm_memcpy(dst: usize, src: usize, len: usize);
}

pub struct Accessor {
    addr: usize,
}

impl Accessor {
    pub fn new(addr: usize) -> Self {
        Self { addr }
    }
    
    pub fn from<T: Copy>(&self) -> &T {
        unsafe { &*(self.addr as *const T) }
    }

    /*
    fn from_raw<T: Copy>(&self) -> *const T {
        self.addr as *const T
    } */

    pub fn from_mut<T: Copy>(&mut self) -> &mut T {
        unsafe { &mut *(self.addr as *mut T) }
    }

    /*
    fn mut_slice<T: Copy>(&mut self, len: usize) -> &mut [T] {
        unsafe {
            let ptr: *mut T = self.from_mut_raw();
            let mut slice = core::slice::from_raw_parts(ptr, len);
        }
    } */

    pub fn from_mut_raw<T: Copy>(&mut self) -> *mut T {
        self.addr as *mut T
    }
}

#[repr(C)]
#[repr(align(4096))]
#[derive(Copy, Clone)]
struct VQCtrl {
    buf: [u8; 2 * 1024 * 1024],
}

#[repr(C)]
#[repr(align(4096))]
#[derive(Copy, Clone)]
struct VQData {
    buf: [u8; 20 * 1024 * 1024],
}

static mut VQ_CTRL: VQCtrl = VQCtrl {
    buf: [0; 2 * 1024 * 1024],
};

static mut VQ_DATA: VQData = VQData {
    buf: [0; 20 * 1024 * 1024],
};

static mut VQ_DATA_PTR: usize = 0;
static mut VQ_CTRL_9P: usize = 0;
static mut VQ_CTRL_NET_TX: usize = 0;
static mut VQ_CTRL_NET_RX: usize = 0;
static mut VQ_CTRL_BLK: usize = 0;
static mut VQ_CTRL_VSOCK_TX: usize = 0;
static mut VQ_CTRL_VSOCK_RX: usize = 0;

const IPA_OFFSET: usize = 0x100000000;
const VQ_START: usize = 0x99600000;
const VQ_DATA_HOST: usize = 0x199600000;
const VQ_CTRL_9P_HOST: usize = VQ_DATA_HOST + (14 * 1024 * 1024);
const VQ_CTRL_NET_TX_HOST: usize = VQ_DATA_HOST + (18 * 1024 * 1024);
const VQ_CTRL_NET_RX_HOST: usize = VQ_DATA_HOST + (22 * 1024 * 1024);
const VQ_CTRL_BLK_HOST: usize = VQ_DATA_HOST + (26 * 1024 * 1024);
const VQ_CTRL_BLK_IN_HOST: usize = VQ_DATA_HOST + (30 * 1024 * 1024);
//const VQ_CTRL_BLK_AES_TAG_HOST: usize = VQ_DATA_HOST + (34 * 1024 * 1024);
//const VQ_CTRL_VSOCK_TX_HOST: usize = VQ_DATA_HOST + (38 * 1024 * 1024);
//const VQ_CTRL_VSOCK_RX_HOST: usize = VQ_DATA_HOST + (42 * 1024 * 1024);
const VIRTQUEUE_NUM: usize = 128;

#[repr(C)]
#[derive(Copy, Clone)]
struct IOVec {
    iov_base: usize,
    iov_len: usize,
}

// p9 structs
#[repr(C)]
#[derive(Copy, Clone)]
struct P9pdu {
    queue_head: u32,
    read_offset: usize,
    write_offset: usize,
    out_iov_cnt: u16,
    in_iov_cnt: u16,
    in_iov: [IOVec; VIRTQUEUE_NUM],
    out_iov: [IOVec; VIRTQUEUE_NUM],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct P9msg {
    size: u32,
    cmd: u8,
    tag: u16,
}

static mut P9PDU: P9pdu = P9pdu {
    queue_head: 0,
    read_offset: 0,
    write_offset: 0,
    out_iov_cnt: 0,
    in_iov_cnt: 0,
    in_iov: [IOVec { iov_base: 0, iov_len: 0 }; VIRTQUEUE_NUM],
    out_iov: [IOVec { iov_base: 0, iov_len: 0 }; VIRTQUEUE_NUM],
};

// net tap device
#[repr(C)]
#[derive(Copy, Clone)]
struct NetTX {
    out_cnt: u32,
    iovs: [IOVec; VIRTQUEUE_NUM],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct NetRX {
    in_cnt: u32,
    iovs: [IOVec; VIRTQUEUE_NUM],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct VirtioNetHdr {
    flags: u8,
    gso_type: u8,
    hdr_len: u16,
    gso_size: u16,
    csum_start: u16,
    csum_offset: u16,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct VirtioNetHdrMgrRxbuf {
    hdr: VirtioNetHdr,
    num_buffers: u16,
}

// blk
#[repr(C)]
#[derive(Copy, Clone)]
struct BlockReq {
    out_cnt: u32,
    in_cnt: u32,
    iovs: [IOVec; VIRTQUEUE_NUM],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct BlockReqHost {
    blk_type: u32,
    cnt: u32,  // iov count
    sector: usize,
    status: usize, 
    iovs: [IOVec; VIRTQUEUE_NUM],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct VirtioBlkOuthdr {
    blk_type: u32,
    ioprio: u32,
    sector: usize,
}

//const VIRTIO_BLK_T_IN: usize = 0;
const VIRTIO_BLK_T_OUT: usize = 1;

fn vq_data() -> usize {
    unsafe { VQ_DATA_PTR }
}
fn vq_ctrl() -> usize {
    unsafe { &VQ_CTRL as *const _ as usize }
}
fn vq_ctrl_9p() -> usize {
    unsafe { VQ_CTRL_9P }
}
fn vq_ctrl_net_tx() -> usize {
    unsafe { VQ_CTRL_NET_TX }
}
/*
fn vq_ctrl_blk() -> usize {
    unsafe { VQ_CTRL_BLK }
} */
/*
fn vq_ctrl_net_rx() -> usize {
    unsafe { VQ_CTRL_NET_RX }
} */
fn vq_ctrl_vsock_tx() -> usize {
    unsafe { VQ_CTRL_VSOCK_TX }
}
fn vq_ctrl_vsock_rx() -> usize {
    unsafe { VQ_CTRL_VSOCK_RX }
}

fn align_to_2mb(addr: usize) -> usize {
    let align_min = 2 * 1024 * 1024;
    let remainder = addr % align_min;
    let aligned_addr = addr + (align_min - remainder);
    aligned_addr
}

pub fn create_memory_cvm_shared() {
    unsafe {
        let ptr = &VQ_DATA as *const _ as usize;
        let aligned_ptr = align_to_2mb(ptr);
        let ctrl_ptr = &VQ_CTRL as *const _ as usize;

        VQ_DATA_PTR = aligned_ptr;
        VQ_CTRL_9P = ctrl_ptr;
        VQ_CTRL_VSOCK_TX = ctrl_ptr + (512 * 1024);
        VQ_CTRL_VSOCK_RX = ctrl_ptr + (800 * 1024);
        VQ_CTRL_NET_TX = ctrl_ptr + (1 * 1024 * 1024);
        VQ_CTRL_NET_RX = ctrl_ptr + (1 * 1024 * 1024) + (512 * 1024);
        VQ_CTRL_BLK = ctrl_ptr + (1 * 1024 * 1024) + (800 * 1024);

        rsi_cloak_create_memory_shared(0, vq_data(), 16 * 1024 * 1024);
        rsi_cloak_create_memory_shared(1, vq_ctrl(), 2 * 1024 * 1024);
    }
}

// virtio common functions
fn copy_iovs(iovs: &[IOVec; VIRTQUEUE_NUM], cnt: usize, to_host_shared: bool) {
    let mut offset: usize;
    let mut len: usize;
    let mut src_addr: usize;
    let mut dst_addr: usize;

    for i in 0..cnt {
        offset = iovs[i].iov_base - VQ_START;
        len = iovs[i].iov_len;

        if to_host_shared {
            src_addr = vq_data() + offset;
            dst_addr = iovs[i].iov_base + IPA_OFFSET;
        } else {
            dst_addr = vq_data() + offset;
            src_addr = iovs[i].iov_base + IPA_OFFSET;
        }

        // ISSUE: without this dummy memcpy, the main copy function is going to cause a data abort. why?
        //   - TODO: solve this issue
        let test_1: usize = 0;
        unsafe {
            asm_memcpy(&test_1 as *const _ as usize, src_addr, 8);
        }

        let test_2: usize = 0;
        unsafe {
            asm_memcpy(dst_addr, &test_2 as *const _ as usize, 8);
        }

        unsafe {
            asm_memcpy(dst_addr, src_addr, len);
        }
    }
}

// p9 (file)
pub fn handle_p9_request() {
    let mut host_acc = Accessor::new(VQ_CTRL_9P_HOST);
    let cvm_acc = Accessor::new(vq_ctrl_9p());
    let p9pdu_host_shared = host_acc.from_mut::<P9pdu>();
    let p9pdu_cvm_shared = cvm_acc.from::<P9pdu>();
    let p9pdu = unsafe { &mut P9PDU };

    // 1. copy iov
    *p9pdu = *p9pdu_cvm_shared;
    *p9pdu_host_shared = *p9pdu;

    // 2. copy data
    copy_iovs(&p9pdu.in_iov, p9pdu.in_iov_cnt as usize, true);
    copy_iovs(&p9pdu.out_iov, p9pdu.out_iov_cnt as usize, true);
}

pub fn handle_p9_response() {
    let p9pdu = unsafe { &mut P9PDU };
    copy_iovs(&p9pdu.in_iov, p9pdu.in_iov_cnt as usize, false);
    copy_iovs(&p9pdu.out_iov, p9pdu.out_iov_cnt as usize, false);
}

#[allow(unused)]
macro_rules! translate_cvm_data {
    ($addr:expr, $len:expr) => {{
        let new_addr = vq_data() + ($addr - VQ_START);
        let mut acc = Accessor::new(new_addr);
        let ptr = acc.from_mut_raw::<u8>();
        let data = unsafe { core::slice::from_raw_parts_mut(ptr, $len) };
        data
    }};
}

// tap (net)
pub fn handle_net_tx_request() {
    let mut host_acc = Accessor::new(VQ_CTRL_NET_TX_HOST);
    let cvm_acc = Accessor::new(vq_ctrl_net_tx());
    let net_tx_host = host_acc.from_mut::<NetTX>();
    let net_tx_cvm = cvm_acc.from::<NetTX>();

    // 1. check
    if net_tx_cvm.out_cnt as usize >= VIRTQUEUE_NUM {
        rsi_print("net_tx_request out_cnt too big", net_tx_cvm.out_cnt as usize, 0);
        return;
    }
    if net_tx_cvm.out_cnt == 0 {
        rsi_print("no net_tx_request", 0, 0);
        return;
    }
    rsi_print("net_tx!", 0, 0);

    // do monitor function
    if net_tx_cvm.out_cnt == 1 {
        rsi_print("net_tx out 1", 0, 0);
        // zero-copy path (udp falls down in this case)
        let data = translate_cvm_data!(net_tx_cvm.iovs[0].iov_base, net_tx_cvm.iovs[0].iov_len);
        let _ = monitor_net_tx(data, 0);
    } else if net_tx_cvm.out_cnt == 2 {
        // copy path (out_cnt == 2) (tcp typically falls down in this case)
        // todo: when it comes with out_cnt being larger than 2?
        let mut merged_data = vec![];
        for i in 0..net_tx_cvm.out_cnt {
            let data = translate_cvm_data!(net_tx_cvm.iovs[i as usize].iov_base, net_tx_cvm.iovs[i as usize].iov_len);
            merged_data.extend_from_slice(data);
        }

        let res = monitor_net_tx(&mut merged_data, 0);
        if res.modified {
            let mut offset: usize = 0;
            for i in 0..net_tx_cvm.out_cnt {
                let len = net_tx_cvm.iovs[i as usize].iov_len;
                let src_slice = &merged_data[offset..offset+len];
                let dst_slice = translate_cvm_data!(net_tx_cvm.iovs[i as usize].iov_base, net_tx_cvm.iovs[i as usize].iov_len);

                for (dst, src) in dst_slice.iter_mut().zip(src_slice) {
                    *dst = *src;
                }
                offset += len;
            }
        }
    } else {
        rsi_print("net_tx_cvm.out_cnt > 2", net_tx_cvm.out_cnt as usize, 0);
    }

    // 2. copy iovs
    net_tx_host.out_cnt = net_tx_cvm.out_cnt;
    for i in 0..net_tx_cvm.out_cnt {
        //net_tx_host.iovs[i as usize].iov_base = net_tx_cvm.iovs[i as usize].iov_base;
        //net_tx_host.iovs[i as usize].iov_len = net_tx_cvm.iovs[i as usize].iov_len;

        let dst_addr = &net_tx_host.iovs[i as usize] as *const _ as usize;
        let src_addr = &net_tx_cvm.iovs[i as usize] as *const _ as usize;
        unsafe {
            asm_memcpy(dst_addr, src_addr, core::mem::size_of::<IOVec>());
        }
    }

    // 3. copy data
    copy_iovs(&net_tx_cvm.iovs, net_tx_cvm.out_cnt as usize, true);
}

pub fn handle_net_rx_response() {
    let len: u32;
    let mut llen: usize;
    let mut iiov: IOVec = IOVec {
        iov_base: 0,
        iov_len: 0,
    };
    let mut ptr_addr: usize = VQ_CTRL_NET_RX_HOST;
    let mut offset: usize;
    let mut dst_addr: usize;

    // read total_len first
    let acc = Accessor::new(ptr_addr);
    len = *(acc.from::<u32>());
    llen = len as usize;
    ptr_addr += core::mem::size_of::<u32>();

    // iov iteration
    while llen > 0 {
        // read iovec
        let iiov_addr = &iiov as *const _ as usize;
        unsafe {
            asm_memcpy(iiov_addr, ptr_addr, core::mem::size_of::<IOVec>());
        }
        ptr_addr += core::mem::size_of::<IOVec>();

        // translate addr
        offset = iiov.iov_base - VQ_START;
        dst_addr = vq_data() + offset;

        if iiov.iov_len > 0 {
            let copy: usize = core::cmp::min(iiov.iov_len, llen);
            
            unsafe {
                asm_memcpy(dst_addr, ptr_addr, copy);
            }

            // do monitor function (udp only now)
            let mut acc = Accessor::new(dst_addr);
            let ptr = acc.from_mut_raw::<u8>();
            let data = unsafe { core::slice::from_raw_parts_mut(ptr, copy) };
            let _ = monitor_net_rx(data, 0);

            ptr_addr += copy;
            llen -= copy;
            iiov.iov_len -= copy;
            iiov.iov_base += copy;
        }
    }
}

pub fn handle_net_rx_num_buffers() {
    let mut ptr_addr: usize = VQ_CTRL_NET_RX_HOST;
    let iov_base: usize;
    let new_addr: usize;
    let num_buffers: u16;

    let acc = Accessor::new(ptr_addr);
    iov_base = *(acc.from::<usize>());
    ptr_addr += core::mem::size_of::<usize>();

    let acc = Accessor::new(ptr_addr);
    num_buffers = *(acc.from::<u16>());

    new_addr = vq_data() + (iov_base - VQ_START);
    let mut acc = Accessor::new(new_addr);
    let hdr = acc.from_mut::<VirtioNetHdrMgrRxbuf>();
    hdr.num_buffers = num_buffers;
}

// blk
pub fn handle_blk() {
    let mut host_acc = Accessor::new(VQ_CTRL_BLK_HOST); // this is a in-out address
    let block_req = host_acc.from_mut::<BlockReq>();

    // copy block_req first to avoid double-fetch situation
    let in_cnt: usize = block_req.in_cnt as usize;
    let out_cnt: usize = block_req.out_cnt as usize;
    let mut iovs: [IOVec; 16] = [
        { IOVec { iov_base: 0, iov_len: 0 } }; 16
    ];
    if in_cnt + out_cnt >= 16 {
        rsi_print("block_req too many", in_cnt, out_cnt);
        return;
    }
    for i in 0..in_cnt + out_cnt {
        iovs[i].iov_len = block_req.iovs[i].iov_len;
        iovs[i].iov_base = block_req.iovs[i].iov_base;
    }

    // parse out header
    let mut last_iov: usize;
    let mut iovcount: usize = out_cnt as usize;
    let out_hdr: VirtioBlkOuthdr = VirtioBlkOuthdr {
        blk_type: 0,
        ioprio: 0,
        sector: 0,
    };
    let mut len: usize = core::mem::size_of::<VirtioBlkOuthdr>();
    let mut copy;
    let mut iov_idx: usize = 0;
    let mut dst_addr: usize = &out_hdr as *const _ as usize;
    let mut src_addr: usize;
    let status: usize;    

    while len > 0 && iovcount > 0 {
        copy = core::cmp::min(len, iovs[iov_idx].iov_len);
        src_addr = vq_data() + (iovs[iov_idx].iov_base - VQ_START);
        unsafe {
            asm_memcpy(dst_addr, src_addr, copy);
        }

        dst_addr += copy;
        len -= copy;

        iovs[iov_idx].iov_base += copy;
        iovs[iov_idx].iov_len -= copy;

        if iovs[iov_idx].iov_len == 0 {
            iov_idx += 1;
            iovcount -= 1;
        }
    }

    /* Extract status byte from iovec */
    iovcount += in_cnt as usize;
	last_iov = iovcount - 1;
	while iovs[iov_idx + last_iov].iov_len == 0 {
		last_iov -= 1;
    }
	iovs[iov_idx + last_iov].iov_len -= 1;
    status = iovs[iov_idx + last_iov].iov_base + iovs[iov_idx + last_iov].iov_len;
	if iovs[iov_idx + last_iov].iov_len == 0 {
		iovcount -= 1;
    }

    // update block_host from now on
    let mut host_acc = Accessor::new(VQ_CTRL_BLK_HOST);
    let block_host = host_acc.from_mut::<BlockReqHost>();

    block_host.blk_type = out_hdr.blk_type;
    block_host.cnt = iovcount as u32;
    block_host.sector = out_hdr.sector;
    block_host.status = status;

    // [JB] update req->status forcefully here (always success) todo: do it later-
    let status_addr = block_host.status + IPA_OFFSET;
    let mut status_acc = Accessor::new(status_addr);
    let status_ptr = status_acc.from_mut::<u8>();
    *status_ptr = 0x00;

    // module check before iov/data copy
    if block_host.blk_type as usize == VIRTIO_BLK_T_OUT {
        for i in 0..iovcount {
            let offset = iovs[iov_idx + i].iov_base - VQ_START;
            let new_addr = vq_data() + offset;
            let len = iovs[iov_idx + i].iov_len;

            let mut acc = Accessor::new(new_addr);
            let ptr = acc.from_mut_raw::<u8>();
            let _data = unsafe { core::slice::from_raw_parts_mut(ptr, len) };

            //monitor_blk_write(data, block_host.sector, VQ_CTRL_BLK_AES_TAG_HOST + core::mem::size_of::<TagStorage>() * block_host.sector);
        }
    }

    // iov metadata copy
    for i in 0..iovcount {
        block_host.iovs[i].iov_base = iovs[iov_idx + i].iov_base;
        block_host.iovs[i].iov_len = iovs[iov_idx + i].iov_len;
    }

    // copy data
    copy_iovs(&block_host.iovs, iovcount, true);
}

pub fn handle_blk_in_resp() {
    let mut host_acc = Accessor::new(VQ_CTRL_BLK_IN_HOST);
    let block_host = host_acc.from_mut::<BlockReqHost>();
    let iovcount: usize = block_host.cnt as usize;

    for i in 0..iovcount {
        let new_addr = block_host.iovs[i].iov_base + IPA_OFFSET;
        let len = block_host.iovs[i].iov_len;

        let mut acc = Accessor::new(new_addr);
        let ptr = acc.from_mut_raw::<u8>();
        let _data = unsafe { core::slice::from_raw_parts_mut(ptr, len) };

        //monitor_blk_read(data, block_host.sector, VQ_CTRL_BLK_AES_TAG_HOST + core::mem::size_of::<TagStorage>() * block_host.sector);
    }

    // copy data
    copy_iovs(&block_host.iovs, iovcount, false);

    // status update
    let offset = block_host.status - VQ_START;
    let new_addr = vq_data() + offset;
    unsafe {
        let ptr = new_addr as *mut u8;
        *ptr = 0x00;  // todo: if len <= 0, setting 1 to *ptr.
    }
}

pub fn __handle_vsock(cvm_addr: usize) {
    // emulate performance overhead
    // [TODO] do the actual job
    let mut dest_tx: NetTX = NetTX {
        out_cnt: 0,
        iovs: [IOVec { iov_base: 0, iov_len: 0 }; VIRTQUEUE_NUM],
    };
    let dest_buffer: [u8; 8192] = [2; 8192];
    let src_buffer: [u8; 8192] = [1; 8192];
    let cvm_acc = Accessor::new(cvm_addr);
    let net_tx_cvm = cvm_acc.from::<NetTX>();

    // 1. check
    if net_tx_cvm.out_cnt as usize >= VIRTQUEUE_NUM {
        rsi_print("vsock out_cnt too big", net_tx_cvm.out_cnt as usize, 0);
        return;
    }
    if net_tx_cvm.out_cnt == 0 {
        rsi_print("no vsock", 0, 0);
        return;
    }

    // 2. copy iovs (emulation)
    dest_tx.out_cnt = net_tx_cvm.out_cnt;
    for i in 0..net_tx_cvm.out_cnt {
        dest_tx.iovs[i as usize].iov_base = net_tx_cvm.iovs[i as usize].iov_base;
        dest_tx.iovs[i as usize].iov_len = net_tx_cvm.iovs[i as usize].iov_len;
    }

    // 3. copy data (emulation)
    for i in 0..dest_tx.out_cnt {
        let mut len = dest_tx.iovs[i as usize].iov_len;
        if len >= 8192 {
            len = 8190;
        }
        unsafe {
            asm_memcpy(&dest_buffer as *const _ as usize, &src_buffer as *const _ as usize, len);
        }
    }
}

pub fn handle_vsock_tx() {
    __handle_vsock(vq_ctrl_vsock_tx());
}

pub fn handle_vsock_rx() {
    __handle_vsock(vq_ctrl_vsock_rx());
}
