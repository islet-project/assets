//use crate::rsi::*;  

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
pub type BlkWriteFunc = fn(&mut [u8], usize, usize) -> ModuleReturn;

// input: bytes to read
// output: ModuleReturn
pub type BlkReadFunc = fn(&mut [u8], usize, usize) -> ModuleReturn;

pub type NetTxFunc = fn(&mut [u8], usize) -> ModuleReturn;
pub type NetRxFunc = fn(&mut [u8], usize) -> ModuleReturn;

pub struct Module {
    #[allow(dead_code)]
    name: &'static str,
    #[allow(dead_code)]
    priority: usize,  // low first. no duplication allowed
    blk_write: BlkWriteFunc,
    blk_read: BlkReadFunc,
    net_tx: NetTxFunc,
    net_rx: NetRxFunc,
}

static mut MODULES: Option<Module> = None;

pub fn add_module(name: &'static str, priority: usize, blk_write: BlkWriteFunc, blk_read: BlkReadFunc,
        net_tx: NetTxFunc, net_rx: NetRxFunc) -> bool {
    let module: Module = Module {
        name,
        priority,
        blk_write,
        blk_read,
        net_tx,
        net_rx,
    };

    // todo: insert() instead of push(), according to "priority"
    // todo: support multiple modules
    unsafe {
        MODULES = Some(module);
    }
    true
}

// root functions
pub fn monitor_blk_write(data: &mut [u8], arg1: usize, arg2: usize) -> ModuleReturn {
    let mut modified: bool = false;

    if let Some(module) = unsafe { &MODULES } {
        let res = (module.blk_write)(data, arg1, arg2);
        match res.action {
            ModuleAction::Deny => panic!("monitor_blk_write denied!"),
            _ => {},
        }
        if res.modified {
            modified = true;
        }
    }

    ModuleReturn {
        modified: modified,
        action: ModuleAction::Allow,
    }
}

pub fn monitor_blk_read(data: &mut [u8], arg1: usize, arg2: usize) -> ModuleReturn {
    let mut modified: bool = false;

    if let Some(module) = unsafe { &MODULES } {
        let res = (module.blk_read)(data, arg1, arg2);
        match res.action {
            ModuleAction::Deny => panic!("blk_read denied!"),
            _ => {},
        }
        if res.modified {
            modified = true;
        }
    }

    ModuleReturn {
        modified: modified,
        action: ModuleAction::Allow,
    }
}

pub fn monitor_net_tx(data: &mut [u8], arg1: usize) -> ModuleReturn {
    let mut modified: bool = false;

    if let Some(module) = unsafe { &MODULES } {
        let res = (module.net_tx)(data, arg1);
        match res.action {
            ModuleAction::Deny => panic!("net_tx denied!"),
            _ => {},
        }
        if res.modified {
            modified = true;
        }
    }

    ModuleReturn {
        modified: modified,
        action: ModuleAction::Allow,
    }
}

pub fn monitor_net_rx(data: &mut [u8], arg1: usize) -> ModuleReturn {
    let mut modified: bool = false;

    if let Some(module) = unsafe { &MODULES } {
        let res = (module.net_rx)(data, arg1);
        match res.action {
            ModuleAction::Deny => panic!("net_rx denied!"),
            _ => {},
        }
        if res.modified {
            modified = true;
        }
    }

    ModuleReturn {
        modified: modified,
        action: ModuleAction::Allow,
    }
}
