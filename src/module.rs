use crate::rsi::*;

pub enum ModuleAction {
    Allow,
    Deny,
}

pub struct ModuleReturn {
    pub modified: bool,
    pub action: ModuleAction,
}

// input: bytes to write
// output: ModuleReturn
pub type BlkWriteFunc = fn(&mut [u8]) -> ModuleReturn;

// input: bytes to read
// output: ModuleReturn
pub type BlkReadFunc = fn(&mut [u8]) -> ModuleReturn;

pub struct Module {
    #[allow(dead_code)]
    name: &'static str,
    #[allow(dead_code)]
    priority: usize,  // low first. no duplication allowed
    blk_write: BlkWriteFunc,
    blk_read: BlkReadFunc,
}

static mut MODULES: Option<Module> = None;

pub fn add_module(name: &'static str, priority: usize, blk_write: BlkWriteFunc, blk_read: BlkReadFunc) -> bool {
    let module: Module = Module {
        name,
        priority,
        blk_write,
        blk_read,
    };

    // todo: insert() instead of push(), according to "priority"
    // todo: support multiple modules
    unsafe {
        MODULES = Some(module);
    }
    true
}

// root functions
pub fn monitor_blk_write(data: &mut [u8]) -> ModuleReturn {
    let mut modified: bool = false;

    if let Some(module) = unsafe { &MODULES } {
        let res = (module.blk_write)(data);
        match res.action {
            ModuleAction::Deny => panic!("monitor_blk_write denied!"),
            _ => {},
        }
        if res.modified {
            modified = true;
        }
    }

    rsi_print("monitor_blk_write passed", data[0] as usize, data[1] as usize);

    ModuleReturn {
        modified: modified,
        action: ModuleAction::Allow,
    }
}

pub fn monitor_blk_read(data: &mut [u8]) -> ModuleReturn {
    let mut modified: bool = false;

    if let Some(module) = unsafe { &MODULES } {
        let res = (module.blk_read)(data);
        match res.action {
            ModuleAction::Deny => panic!("blk_read denied!"),
            _ => {},
        }
        if res.modified {
            modified = true;
        }
    }

    rsi_print("monitor_blk_read passed", data[0] as usize, data[1] as usize);

    ModuleReturn {
        modified: modified,
        action: ModuleAction::Allow,
    }
}
