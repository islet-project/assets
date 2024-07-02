use crate::module::*;
use crate::aes::{self, TagStorage};
use crate::virtio::Accessor;
use crate::rsi::*;
use alloc::vec::Vec;

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

pub fn blk_write(data: &mut [u8], _arg1: usize, arg2: usize) -> ModuleReturn {
    //let sector = arg1;
    let tag_addr = arg2;
    let mut tag_acc = Accessor::new(tag_addr);
    let tag_storage = tag_acc.from_mut::<TagStorage>();

    let _ = aes::encrypt(data, tag_storage);

    ModuleReturn {
        modified: true,
        action: ModuleAction::Allow,
    }
}

pub fn blk_read(data: &mut [u8], _arg1: usize, arg2: usize) -> ModuleReturn {
    //let sector = arg1;
    let tag_addr = arg2;
    let mut tag_acc = Accessor::new(tag_addr);
    let tag_storage = tag_acc.from_mut::<TagStorage>();

    let _ = aes::decrypt(data, tag_storage);

    ModuleReturn {
        modified: true,
        action: ModuleAction::Allow,
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

fn encrypt_pkt(data: &mut [u8], is_encrypt: bool) -> bool {
    let modulus = data.len() % 16;
    if modulus == 0 {
        if is_encrypt {
            return aes::encrypt_no_auth(data);
        } else {
            return aes::decrypt_no_auth(data);
        }
    } else {
        let adder = 16 - modulus;
        let mut padded_data = Vec::new();
        for v in data.iter() {
            padded_data.push(*v);
        }
        for _ in 0..adder {
            padded_data.push(0x0);
        }

        if is_encrypt {
            if aes::encrypt_no_auth(&mut padded_data) == false {
                return false;
            }
        } else {
            if aes::decrypt_no_auth(&mut padded_data) == false {
                return false;
            }
        }

        for (dst, src) in data.iter_mut().zip(&mut padded_data) {
            *dst = *src;
        }
    }

    true
}

pub fn net_tx(data: &mut [u8], _arg1: usize) -> ModuleReturn {
    // en/decrypt tcp/udp packets only
    let res = is_tcp_or_udp(data);
    if res.0 == IPV4_TCP {
        if res.1 >= data.len() {
            rsi_print("overflow in net_tx", 0, 0);
        }

        let tcp_data = &mut data[res.1..];
        if encrypt_pkt(tcp_data, true) == false {
            rsi_print("tcp tx enc error", 0, 0);
        }

        ModuleReturn {
            modified: true,
            action: ModuleAction::Allow,
        }
    } 
    else if res.0 == IPV4_UDP {
        if res.1 >= data.len() {
            rsi_print("overflow in net_tx", 0, 0);
        }

        // encryption
        let udp_data = &mut data[res.1..];
        if encrypt_pkt(udp_data, true) == false {
            rsi_print("udp tx enc error", 0, 0);
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

pub fn net_rx(data: &mut [u8], _arg1: usize) -> ModuleReturn {
    // en/decrypt tcp/udp packets only
    let res = is_tcp_or_udp(data);
    if res.0 == IPV4_TCP {
        if res.1 >= data.len() {
            rsi_print("overflow in net_rx", 0, 0);
        }

        // decryption
        let tcp_data = &mut data[res.1..];
        if encrypt_pkt(tcp_data, false) == false {
            rsi_print("tcp rx dec error", 0, 0);
        }

        ModuleReturn {
            modified: true,
            action: ModuleAction::Allow,
        }
    }
    else if res.0 == IPV4_UDP {
        if res.1 >= data.len() {
            rsi_print("overflow in net_rx", 0, 0);
        }

        // decryption
        let udp_data = &mut data[res.1..];
        if encrypt_pkt(udp_data, false) == false {
            rsi_print("udp rx dec error", 0, 0);
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
