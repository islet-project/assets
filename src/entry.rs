use crate::def::*;
use crate::rsi::*;
use core::ptr::addr_of;

static mut PRIMARY_MPIDR: usize = INVALID_MPIDR;

core::arch::global_asm!(include_str!("vtable.S"));
core::arch::global_asm!(include_str!("symbols.S"));
extern "C" {
    pub static mut vector_table: u64;
    pub static mut dummy_stack_end: u64;
}

extern "C" {
    static __TEXT_START__: usize;
    static __TEXT_END__: usize;
    static __RODATA_START__: usize;
    static __RODATA_END__: usize;
    static __DATA_START__: usize;
    static __DATA_END__: usize;
    static __BSS_START__: usize;
    static __BSS_END__: usize;
}

#[link_section = ".text"]
#[allow(unused)]
#[no_mangle]
unsafe extern "C" fn get_primary_mpidr() -> usize {
    addr_of!(PRIMARY_MPIDR) as usize
}

#[naked]
#[link_section = ".head.text"]
#[no_mangle]
unsafe extern "C" fn gateway_entry() -> ! {
    core::arch::asm!("
        /* Install vector table */
        adrp  x0, vector_table
        msr  vbar_el1, x0
    
        /* Set x19 = 1 for primary cpu
        * Set x19 = 0 for secondary cpu
        */
        bl get_primary_mpidr
        mov x18, x0
        ldr x0, [x18]
        mov   x2, {}
        cmp   x2, x0
        b.ne  2f
    
        mov   x19, #1

        /* Enable I-Cache */
        mrs  x0, sctlr_el1
        orr  x0, x0, {}
        msr  sctlr_el1, x0
        isb

        /* Save the primary cpu mpidr */
        bl get_primary_mpidr
        mov x18, x0
        mrs x0, mpidr_el1
        str x0, [x18]

        /* Clear BSS */
        adrp x0, __BSS_START__
        adrp x1, __BSS_END__
        sub x1, x1, x0
    1:
        stp xzr, xzr, [x0]
        add x0, x0, #16
        sub x1, x1, #16
        cmp xzr, x1
        b.ne 1b

        b  0f

    2:  // secondary_cpu_entry
        mov   x19, #0

        /* Enable I-Cache */
        mrs  x0, sctlr_el1
        orr  x0, x0, {}
        msr  sctlr_el1, x0
        isb

    0:
        /* Setup the dummy stack to call val_get_cpuid C fn */
        adrp  x1, dummy_stack_end
        mov  sp, x1
    
        mrs  x0, mpidr_el1
        bl   get_cpu_id
    
        /* Now setup the stack pointer with actual stack addr
        * for the logic cpuid return by val_get_cpuid
        */
        adrp  x1, stacks_end
        mov  x2, {}
        mul  x2, x0, x2
        sub  sp, x1, x2
     
        /* And jump to the C entrypoint. */
        mov x0, x19
        b      main",
        const INVALID_MPIDR,
        const SCTLR_I_BIT,
        const SCTLR_I_BIT,
        const STACK_SIZE,
        options(noreturn)
    )
}

#[link_section = ".text"]
#[allow(unused)]
#[no_mangle]
unsafe extern "C" fn get_cpu_id(mp: usize) -> usize {
    let total_cpu_num = NUM_OF_CPU;
    let mut mpidr = mp;

    mpidr = mpidr & PAL_MPIDR_AFFINITY_MASK;

    for idx in 0..total_cpu_num {
        if mpidr == PHY_MPIDR_ARRAY[idx] {
            return idx as usize;
        }
    }
    for idx in 0..total_cpu_num {
        if mpidr == idx {
            return idx as usize;
        }
    }

    PAL_INVALID_MPID
}

#[no_mangle]
fn esr_el1() -> usize {
    let mut val: usize;
    unsafe { core::arch::asm!("mrs {}, esr_el1", out(reg) val); }
    val
}

#[no_mangle]
fn far_el1() -> usize {
    let mut val: usize;
    unsafe { core::arch::asm!("mrs {}, far_el1", out(reg) val); }
    val
}

#[no_mangle]
fn elr_el1() -> usize {
    let mut val: usize;
    unsafe { core::arch::asm!("mrs {}, elr_el1", out(reg) val); }
    val
}

#[no_mangle]
#[allow(unused_variables)]
pub extern "C" fn sync_exception_current() -> ! {
    rsi_print("sync_exception_current", 0, 0);
    rsi_print("esr_el1", esr_el1(), 0);
    rsi_print("far_el1", far_el1(), 0);
    rsi_print("elr_el1", elr_el1(), 0);

    panic!("ddd");
    //0  // false
}

#[no_mangle]
#[allow(unused_variables)]
pub extern "C" fn irq_current() -> usize {
    rsi_print("irq_current", 0, 0);
    1  // true
}

#[panic_handler]
pub fn panic_handler(_info: &core::panic::PanicInfo<'_>) -> ! {
    loop {}
}
