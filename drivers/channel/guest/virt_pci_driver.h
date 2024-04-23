#ifndef _VIRT_PCI_DRIVER_H
#define _VIRT_PCI_DRIVER_H

typedef enum {
	SHRM_ALLOCATOR = 0, // It's host module
	SERVER = 1,
	CLIENT = 2,
} ROLE;

s64 mmio_read_to_get_shrm(SHRM_TYPE shrm_type);
int mmio_write_to_remove_shrm(u64 ipa);
int mmio_write_to_unmap_shrm(u64 ipa);
int mmio_write_to_get_ro_shrm(u32 shrm_id);
u64* get_shrm_va(SHRM_TYPE shrm_type, u64 ipa);

#endif /* _VIRT_PCI_DRIVER_H */
