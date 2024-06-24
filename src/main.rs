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
pub mod module;
pub mod module_cvm_hardening;
pub mod allocator;
pub mod aes;

use crate::rsi::*;
use crate::virtio::*;
use crate::module::*;
use crate::module_cvm_hardening as cvm_hardening;

extern crate alloc;

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
            },
            _ => {
                rsi_print("unsupported msg type", msg_type, 0);
            },
        }

        outlen = 0;
    }
}

/*
fn test_aes() {
    let key_bytes: [u8; 32] = [0; 32];
    let nonce_bytes: [u8; 12] = [0; 12];
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);
    let cipher = Aes256Gcm::new(&key);
    let nonce = Nonce::from_slice(&nonce_bytes);
    let mut data: [u8; 64] = [0; 64];
    let payload = Payload {
        msg: &mut data,
        aad: b"",
    };

    let mut ciphertext = cipher.encrypt(nonce, payload).unwrap();
    let plaintext = cipher.decrypt(nonce, ciphertext.as_ref()).unwrap();
    
    // encryption test
    rsi_print("before enc_check", 0, 0);
    assert_eq!(&plaintext, b"plaintext message");
    rsi_print("after enc_check", 0, 0);

    // tag match test
    <Vec<u8> as AsMut<[u8]>>::as_mut(&mut ciphertext)[1] = 0x77; // modification
    match cipher.decrypt(nonce, ciphertext.as_ref()) {
        Ok(_) => rsi_print("tag match!", 0, 0),
        Err(_) => rsi_print("tag mismatch!", 0, 0),
    }
} */

#[no_mangle]
pub unsafe fn main() {
    // 1. IPA state set
    rsi_set_memory_realm(0x80000000, 0x90000000);
    rsi_set_memory_host_shared(0x88400000, 0x8c400000);

    // 2. Create a shared memory with CVM_App
    create_memory_cvm_shared();

    // 3. memory configuration (todo)
    allocator::init();  // initialize heap memory

    // 4. register modules
    let _ = add_module("cvm_hardening", 0, cvm_hardening::blk_write, cvm_hardening::blk_read);

    // 5. Main loop
    gateway_main_loop();
}
