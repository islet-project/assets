pub const INVALID_MPIDR: usize = 0xffffffff;
pub const SCTLR_I_BIT: usize = 1 << 12;
pub const STACK_SIZE: usize = 0x1000;
pub const NUM_OF_CPU: usize = 8;

pub const PHY_MPIDR_ARRAY: [usize; NUM_OF_CPU] = [
    0x00000,  // cpu 0
    0x00100,  // cpu 1
    0x00200,
    0x00300,
    0x10000,
    0x10100,
    0x10200,
    0x10300
];
pub const PAL_INVALID_MPID: usize = 0xFFFFFFFF;
const PAL_MPIDR_AFFLVL_MASK: usize = 0xff;

const PAL_MPIDR_AFF0_SHIFT: usize = 0;
const PAL_MPIDR_AFF1_SHIFT: usize = 8;
const PAL_MPIDR_AFF2_SHIFT: usize = 16;
const PAL_MPIDR_AFF3_SHIFT: usize = 32;

pub const PAL_MPIDR_AFFINITY_MASK: usize = (PAL_MPIDR_AFFLVL_MASK << PAL_MPIDR_AFF3_SHIFT) |
                 (PAL_MPIDR_AFFLVL_MASK << PAL_MPIDR_AFF2_SHIFT) |
                 (PAL_MPIDR_AFFLVL_MASK << PAL_MPIDR_AFF1_SHIFT) |
                 (PAL_MPIDR_AFFLVL_MASK << PAL_MPIDR_AFF0_SHIFT);