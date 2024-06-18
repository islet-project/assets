#![no_std]
#![no_main]
#![feature(asm_const)]
#![feature(naked_functions)]
#![warn(rust_2018_idioms)]
#![deny(warnings)]

pub mod entry;
pub mod def;
pub mod rsi;
pub mod virtio;

use crate::rsi::*;
use crate::virtio::*;

const FIRST_CLOAK_OUTLEN: usize = 999999;
const CLOAK_MSG_TYPE_P9: usize = 2;
const CLOAK_MSG_TYPE_P9_RESP: usize = 12;

const CLOAK_MSG_TYPE_NET_TX: usize = 3;
const CLOAK_MSG_TYPE_NET_RX: usize = 4;
const CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS: usize = 5;
const CLOAK_MSG_TYPE_NET_RX_RESP: usize = 14;
const CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS_RESP: usize = 15;
const CLOAK_MSG_TYPE_BLK_IN_RESP: usize = 16;

const CLOAK_MSG_TYPE_BLK: usize = 6;
const CLOAK_MSG_TYPE_BLK_IN: usize = 7;

fn gateway_main_loop() -> ! {
    let mut outlen: usize = FIRST_CLOAK_OUTLEN;
    let mut msg_type: usize = 0;

    loop {
        // 1. host_call to wait for a request at Host
        rsi_cloak_host_call(outlen, &mut msg_type);

        // 2. handle a request
        match msg_type {
            CLOAK_MSG_TYPE_P9 => {
                handle_p9_request();
            },
            CLOAK_MSG_TYPE_P9_RESP => {
                handle_p9_response();
            },
            CLOAK_MSG_TYPE_NET_TX => {
                handle_net_tx_request();
            },
            CLOAK_MSG_TYPE_NET_RX => {
                // it shouldn't be called
                rsi_print("CLOAK_MSG_TYPE_NET_RX called", 0, 0);
            },
            CLOAK_MSG_TYPE_NET_RX_RESP => {
                handle_net_rx_response();
            },
            CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS => {
                // it shouldn't be called
                rsi_print("CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS called", 0, 0);
            },
            CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS_RESP => {
                handle_net_rx_num_buffers();
            },
            CLOAK_MSG_TYPE_BLK => {
                handle_blk();
            },
            CLOAK_MSG_TYPE_BLK_IN => {
                // it shouldn't be called
                rsi_print("CLOAK_MSG_TYPE_BLK_IN called", 0, 0);
            },
            CLOAK_MSG_TYPE_BLK_IN_RESP => {
                handle_blk_in_resp();
            }
            _ => {
                rsi_print("unsupported msg type", msg_type, 0);
            },
        }

        outlen = 0;
    }
}

#[no_mangle]
pub unsafe fn main() {
    // 1. IPA state set
    rsi_set_memory_realm(0x80000000, 0x90000000);
    rsi_set_memory_host_shared(0x88400000, 0x8c400000);

    // 2. Create a shared memory with CVM_App
    create_memory_cvm_shared();

    // 3. MMU configuration (todo)

    // 4. Main loop
    gateway_main_loop();
}
