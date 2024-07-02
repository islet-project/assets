use crate::rsi::*;
use aes_gcm::{
    aead::{Aead, KeyInit, Payload},
    Aes256Gcm, Key, Nonce
};
use aes::Aes128;
use aes::cipher::{
    BlockEncrypt, BlockDecrypt,
    generic_array::GenericArray,
};
use alloc::vec::Vec;
const TAG_SIZE: usize = 16;

// todo: currently, nonce and key are a fixed value (AES256 GCM)

#[repr(C)]
#[derive(Copy, Clone)]
pub struct TagStorage {
    pub tag: [u8; 16],
}

pub fn encrypt(data: &mut [u8], tag: &mut TagStorage) -> bool {
    let key_bytes: [u8; 32] = [0; 32];
    let nonce_bytes: [u8; 12] = [0; 12];
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);
    let cipher = Aes256Gcm::new(&key);
    let nonce = Nonce::from_slice(&nonce_bytes);
    let payload = Payload {
        msg: data,
        aad: b"",
    };

    if let Ok(ciphertext) = cipher.encrypt(nonce, payload) {
        let (ct, auth_tag) = ciphertext.split_at(ciphertext.len() - TAG_SIZE);
        
        // data copy
        for (dst, src) in data.iter_mut().zip(ct) {
            *dst = *src;
        }

        // tag copy
        for (dst, src) in tag.tag.iter_mut().zip(auth_tag) {
            *dst = *src;
        }
        true
    } else {
        rsi_print("encrypt fail!", 0, 0);
        false
    }
}

pub fn decrypt(data: &mut [u8], tag: &mut TagStorage) -> bool {
    let key_bytes: [u8; 32] = [0; 32];
    let nonce_bytes: [u8; 12] = [0; 12];
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);
    let cipher = Aes256Gcm::new(&key);
    let nonce = Nonce::from_slice(&nonce_bytes);

    let mut data_with_tag: Vec<u8> = Vec::new();
    for v in data.iter() {
        data_with_tag.push(*v);
    }
    for v in tag.tag.iter() {
        data_with_tag.push(*v);
    }

    let payload = Payload {
        msg: data_with_tag.as_ref(),
        aad: b"",
    };

    if let Ok(plaintext) = cipher.decrypt(nonce, payload) {
        // data copy
        let pt = <Vec<u8> as AsRef<[u8]>>::as_ref(&plaintext);
        for (dst, src) in data.iter_mut().zip(pt) {
            *dst = *src;
        }
        true
    } else {
        // no copy
        rsi_print("decrypt fail!", 0, 0);
        false
    }
}

fn __encrypt_no_auth(data: &mut [u8], is_encrypt: bool) -> bool {
    let key = GenericArray::from([0u8; 16]);
    let cipher = Aes128::new(&key);
    let len = data.len();
    let mut curr = 0;

    if data.len() % 16 != 0 {
        return false;
    }

    // blocks
    while curr < len {
        let mut d: [u8; 16] = [0; 16];
        for (dst, src) in d.iter_mut().zip(&data[curr..curr+16]) {
            *dst = *src;
        }
        let mut block = GenericArray::from(d);

        if is_encrypt {
            cipher.encrypt_block(&mut block);
        } else {
            cipher.decrypt_block(&mut block);
        }

        for (dst, src) in (&mut data[curr..curr+16]).iter_mut().zip(&block) {
            *dst = *src;
        }

        curr += 16;
    }

    true
}

pub fn encrypt_no_auth(data: &mut [u8]) -> bool {
    __encrypt_no_auth(data, true)
}

pub fn decrypt_no_auth(data: &mut [u8]) -> bool {
    __encrypt_no_auth(data, false)
}
