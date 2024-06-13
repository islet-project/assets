
core::arch::global_asm!(include_str!("smc.S"));
extern "C" {
    fn smc_call_asm(param: usize);
}

// RSI calls
const RSI_IPA_STATE_SET: usize = 0xC4000197;
const RSI_CLOAK_PRINT_CALL: usize = 0xC4000204;
const RSI_CHANNEL_CREATE: usize = 0xC4000200;
const RSI_HOST_CALL: usize = 0xC4000199;

// Other definitions
const RSI_RIPAS_EMPTY: usize = 0;
const RSI_RIPAS_RAM: usize = 1;

// Cloak-specific definitions
const CLOAK_HOST_CALL: u16 = 799;

#[repr(C)]
#[derive(Copy, Clone)]
struct SmcParam {
    x0: usize,
    x1: usize,
    x2: usize,
    x3: usize,
    x4: usize,
    x5: usize,
    x6: usize,
    x7: usize,
    x8: usize,
    x9: usize,
    x10: usize,
}

#[repr(C)]
#[repr(align(4096))]
#[derive(Copy, Clone)]
struct PrintCall {
    msg: [u8; 1024],
    data1: usize,
    data2: usize,
}

#[repr(C)]
#[repr(align(4096))]
#[derive(Copy, Clone)]
struct HostCallArg {
    imm: u16,
    gprs: [usize; 7],
}

static mut PRINT_CALL_DATA: PrintCall = PrintCall {
    msg: [0; 1024],
    data1: 0,
    data2: 0,
};

static mut HOST_CALL_DATA: HostCallArg = HostCallArg {
    imm: 0,
    gprs: [0; 7],
};

fn host_call_data() -> usize {
    unsafe { &HOST_CALL_DATA.imm as *const _ as usize }
}

fn smc_call(x0: usize, x1: usize, x2: usize, x3: usize, x4: usize,
            x5: usize, x6: usize, x7: usize, x8: usize, x9: usize, x10: usize) -> SmcParam
{
    let param = SmcParam {
        x0, x1, x2, x3, x4, x5,
        x6, x7, x8, x9, x10
    };
    unsafe {
        smc_call_asm(&param as *const _ as usize);
    }
    param
}

pub fn rsi_print(msg: &str, data1: usize, data2: usize) {
    let msg_bytes = msg.as_bytes();
    let len = msg_bytes.len();

    if len >= 1024 - 1 {
        return;
    }

    unsafe {
        PRINT_CALL_DATA.msg.fill(0);
        for (dst, src) in PRINT_CALL_DATA.msg.iter_mut().zip(msg_bytes) {
            *dst = *src;
        }
        PRINT_CALL_DATA.data1 = data1;
        PRINT_CALL_DATA.data2 = data2;

        let _ = smc_call(RSI_CLOAK_PRINT_CALL, &PRINT_CALL_DATA as *const _ as usize, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

fn rsi_set_memory_range(start: usize, end: usize, ripas: usize) {
    let mut top: usize;
    let mut curr = start;

    while curr != end {
        let res = smc_call(RSI_IPA_STATE_SET, start, end - start, ripas, 0, 0, 0, 0, 0, 0, 0);
        top = res.x1;
        curr = top;
    }
}

pub fn rsi_set_memory_realm(start: usize, end: usize) {
    rsi_set_memory_range(start, end, RSI_RIPAS_RAM);
}

pub fn rsi_set_memory_host_shared(start: usize, end: usize) {
    rsi_set_memory_range(start, end, RSI_RIPAS_EMPTY);
}

pub fn rsi_cloak_create_memory_shared(id: usize, ipa: usize, size: usize) {
    let _ = smc_call(RSI_CHANNEL_CREATE, id, ipa, size, 0, 0, 0, 0, 0, 0, 0);
}

pub fn rsi_cloak_host_call(outlen: usize, msg_type: &mut usize) {
    unsafe {
        HOST_CALL_DATA.gprs.fill(0);
        HOST_CALL_DATA.imm = CLOAK_HOST_CALL;
        HOST_CALL_DATA.gprs[0] = outlen;

        let _ = smc_call(RSI_HOST_CALL, host_call_data(), 0, 0, 0, 0, 0, 0, 0, 0, 0);

        *msg_type = HOST_CALL_DATA.gprs[6];
    }
}