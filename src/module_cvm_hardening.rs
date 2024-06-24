use crate::module::*;
use crate::aes::{self, TagStorage};
use crate::virtio::Accessor;

pub fn blk_write(data: &mut [u8], _arg1: usize, arg2: usize) -> ModuleReturn {
    //let sector = arg1;
    let tag_addr = arg2;
    let mut tag_acc = Accessor::new(tag_addr);
    let tag_storage = tag_acc.from_mut::<TagStorage>();

    let _ = aes::encrypt(data, tag_storage);

    ModuleReturn {
        modified: false,
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
        modified: false,
        action: ModuleAction::Allow,
    }
}
