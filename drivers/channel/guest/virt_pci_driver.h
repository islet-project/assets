#ifndef _VIRT_PCI_DRIVER_H
#define _VIRT_PCI_DRIVER_H

s64 mmio_read_to_get_shrm(void);
int mmio_write_to_remove_shrm(u64 ipa);
int mmio_write_to_unmap_shrm(u64 ipa);

#endif /* _VIRT_PCI_DRIVER_H */
