use core::mem::MaybeUninit;
use linked_list_allocator::LockedHeap;
use core::ptr::addr_of_mut;

const HEAP_SIZE: usize = 4 * 1024 * 1024;

static mut HEAP: [MaybeUninit<u8>; HEAP_SIZE] = [MaybeUninit::uninit(); HEAP_SIZE];

#[global_allocator]
static mut ALLOCATOR: LockedHeap = LockedHeap::empty();

pub unsafe fn init() {
    ALLOCATOR.lock().init_from_slice(&mut *addr_of_mut!(HEAP));
}

pub fn get_used_size() -> usize {
    unsafe { ALLOCATOR.lock().used() }
}
