use crate::module::*;

pub fn blk_write(_data: &mut [u8]) -> ModuleReturn {
    ModuleReturn {
        modified: false,
        action: ModuleAction::Allow,
    }
}

pub fn blk_read(_data: &mut [u8]) -> ModuleReturn {
    ModuleReturn {
        modified: false,
        action: ModuleAction::Allow,
    }
}
