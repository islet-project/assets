use crate::module::*;
use crate::rsi::*;
use core::f32::consts::PI;

const VNET_HDR_SIZE: usize = 12;
const VNET_DATA_OFFSET: usize = VNET_HDR_SIZE;
const ETH_HDR_SIZE: usize = 14;
const ETH_DATA_OFFSET: usize = VNET_DATA_OFFSET + ETH_HDR_SIZE;
const IPV4_HDR_SIZE: usize = 20;
const IPV4_DATA_OFFSET: usize = ETH_DATA_OFFSET + IPV4_HDR_SIZE;
const UDP_HDR_SIZE: usize = 8;
const UDP_DATA_OFFSET: usize = IPV4_DATA_OFFSET + UDP_HDR_SIZE;

const ETH_IPV4: u16 = 0x800;
const IPV4_TCP: u8 = 0x6;
const IPV4_UDP: u8 = 0x11;

extern "C" {
    fn asm_memcpy(dst: usize, src: usize, len: usize);
}

// guassian noise generation function
fn sqrt_approx(x: f32) -> f32 {
    if x == 0.0 { return 0.0; }
    let mut z = x / 2.0;
    for _ in 0..5 {
        z = (z + x / z) / 2.0;
    }
    z
}

fn ln_approx(x: f32) -> f32 {
    if x <= 0.0 { return 0.0; }
    let y = (x - 1.0) / (x + 1.0);
    let y2 = y * y;
    let mut result = 2.0 * y;
    let mut term = y * y2;

    for i in (1..10).step_by(2) {
        result += 2.0 * term / (i as f32 + 2.0);
        term *= y2;
    }

    result
}

fn cos_approx(x: f32) -> f32 {
    let mut term = 1.0;
    let mut result = term;
    let x2 = x * x;
    let mut sign = -1.0;

    for i in (2..12).step_by(2) {
        term *= x2 / ((i * (i - 1)) as f32);
        result += sign * term;
        sign *= -1.0;
    }

    result
}

fn sin_approx(x: f32) -> f32 {
    let mut term = x;
    let mut result = term;
    let x2 = x * x;
    let mut sign = -1.0;

    for i in (3..13).step_by(2) {
        term *= x2 / ((i * (i - 1)) as f32);
        result += sign * term;
        sign *= -1.0;
    }

    result
}

struct SimpleRNG {
    seed: u32,
}

impl SimpleRNG {
    fn new(seed: u32) -> Self {
        SimpleRNG { seed }
    }

    fn next(&mut self) -> f32 {
        self.seed = self.seed.wrapping_mul(1664525).wrapping_add(1013904223);
        (self.seed >> 9) as f32 / (1u64 << 23) as f32
    }
}

fn box_muller(rng: &mut SimpleRNG) -> (f32, f32) {
    let u1 = rng.next();
    let u2 = rng.next();
    
    let z0 = sqrt_approx(-2.0 * ln_approx(u1)) * cos_approx(2.0 * PI * u2);
    let z1 = sqrt_approx(-2.0 * ln_approx(u1)) * sin_approx(2.0 * PI * u2);
    (z0, z1)
}

fn gaussian_noise(rng: &mut SimpleRNG, std_dev: f32) -> (f32, f32) {
    let (z0, z1) = box_muller(rng);
    (z0 * std_dev, z1 * std_dev)
}

// do not allow block requests
pub fn blk_write(_data: &mut [u8], _arg1: usize, _arg2: usize) -> ModuleReturn {
    ModuleReturn {
        modified: false,
        action: ModuleAction::Deny,
    }
}

pub fn blk_read(_data: &mut [u8], _arg1: usize, _arg2: usize) -> ModuleReturn {
    ModuleReturn {
        modified: false,
        action: ModuleAction::Deny,
    }
}

// return value (bool, usize) --> (protocol_id, data_start_idx)
fn is_tcp_or_udp(data: &[u8]) -> (u8, usize) {
    // eth header check
    if data.len() < VNET_DATA_OFFSET + 14 {
        rsi_print("no eth frame", 0, 0);
        return (0, 0);
    }

    let pkt = &data[VNET_DATA_OFFSET+12..];
    let mut eth_type = [0u8; 2];
    eth_type.clone_from_slice(&pkt[0..2]);
    if u16::from_be_bytes(eth_type) != ETH_IPV4 {
        return (0, 0);
    }

    // ipv4 header check (35)
    if data.len() < ETH_DATA_OFFSET + 9 {
        rsi_print("no ipv4 pkt", 0, 0);
        return (0, 0);
    }

    let pkt = &data[ETH_DATA_OFFSET+9..];
    let ip_next = pkt[0];

    match ip_next {
        IPV4_TCP => {
            if data.len() < IPV4_DATA_OFFSET + 12 {
                rsi_print("no tcp pkt", 0, 0);
                return (0, 0);
            }

            let pkt = &data[IPV4_DATA_OFFSET+12..];
            let pkt_size = (((pkt[0] & 0xf0) >> 4) * 4) as usize;

            if data.len() < IPV4_DATA_OFFSET + pkt_size {
                rsi_print("no tcp data", 0, 0);
                return (0, 0);
            }

            (IPV4_TCP, IPV4_DATA_OFFSET + pkt_size)
        },
        IPV4_UDP => {
            if data.len() < UDP_DATA_OFFSET {
                rsi_print("no udp data", 0, 0);
                return (0, 0);
            }

            (IPV4_UDP, UDP_DATA_OFFSET)
        },
        _ => {
            (0, 0)
        },
    }
}

pub fn net_tx(data: &mut [u8], _arg1: usize) -> ModuleReturn {
    let res = is_tcp_or_udp(data);
    if res.0 == IPV4_TCP {
        // don't care TCP now
        ModuleReturn {
            modified: false,
            action: ModuleAction::Allow,
        }
    } 
    else if res.0 == IPV4_UDP {
        if res.1 >= data.len() {
            rsi_print("overflow in net_tx", 0, 0);
        }

        // inject gaussian noise here
        // we assume a fixed ML model structure and apply noises into bias only for PoC
        let udp_data = &mut data[res.1..];
        if udp_data.len() != 6916 {
            // hard-coded one: do not care about other packets- only cares a FL local model now..
            return ModuleReturn {
                modified: false,
                action: ModuleAction::Allow,
            };
        }

        let mut rng = SimpleRNG::new(123456789);
        for idx in (0x112..0x182).step_by(4) { // 0x112~0x182: the location of bias
            let (noise, _) = gaussian_noise(&mut rng, 0.01);
            let val: f32 = 0.0;
            unsafe { asm_memcpy(&val as *const _ as usize, &udp_data[idx] as *const _ as usize, 4) };  // read
            let sum: f32 = val + noise;
            unsafe { asm_memcpy(&udp_data[idx] as *const _ as usize, &sum as *const _ as usize, 4) };  // write back
        }

        ModuleReturn {
            modified: true,
            action: ModuleAction::Allow,
        }
    }
    else {
        ModuleReturn {
            modified: false,
            action: ModuleAction::Allow,
        }
    }
}

pub fn net_rx(_data: &mut [u8], _arg1: usize) -> ModuleReturn {
    ModuleReturn {
        modified: false,
        action: ModuleAction::Allow,
    }
}

pub fn test_noise() {
    let data: [u8; 6916] = [0; 6916];
    let mut rng = SimpleRNG::new(123456789);
    for idx in (0x112..0x182).step_by(4) { // 0x112~0x182: the location of bias
        rsi_print("test-idx", idx, 0);
        let (noise, _) = gaussian_noise(&mut rng, 0.01);
        let val: f32 = 0.0;
        unsafe { asm_memcpy(&val as *const _ as usize, &data[idx] as *const _ as usize, 4) };  // read
        let sum: f32 = val + noise;
        unsafe { asm_memcpy(&data[idx] as *const _ as usize, &sum as *const _ as usize, 4) };  // write back
        rsi_print("test-idx-sum", idx, sum as usize);
    }
    rsi_print("end test_noise", 0, 0);
}
