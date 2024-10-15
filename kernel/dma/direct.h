/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 Christoph Hellwig.
 *
 * DMA operations that map physical memory directly without using an IOMMU.
 */
#ifndef _KERNEL_DMA_DIRECT_H
#define _KERNEL_DMA_DIRECT_H

#include <linux/dma-direct.h>
#include <linux/memremap.h>

bool dma_direct_can_mmap(struct device *dev);
bool dma_direct_need_sync(struct device *dev, dma_addr_t dma_addr);
int dma_direct_map_sg(struct device *dev, struct scatterlist *sgl, int nents,
		enum dma_data_direction dir, unsigned long attrs);
size_t dma_direct_max_mapping_size(struct device *dev);

void dma_direct_sync_single_for_device(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (unlikely(is_swiotlb_buffer(dev, paddr)))
		swiotlb_sync_single_for_device(dev, paddr, size, dir);

	if (!dev_is_dma_coherent(dev))
		arch_sync_dma_for_device(paddr, size, dir);
}

void dma_direct_sync_single_for_cpu(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (!dev_is_dma_coherent(dev)) {
		arch_sync_dma_for_cpu(paddr, size, dir);
		arch_sync_dma_for_cpu_all();
	}

	if (unlikely(is_swiotlb_buffer(dev, paddr)))
		swiotlb_sync_single_for_cpu(dev, paddr, size, dir);

	if (dir == DMA_FROM_DEVICE)
		arch_dma_mark_clean(paddr, size);
}

dma_addr_t dma_direct_map_page(struct device *dev,
		struct page *page, unsigned long offset, size_t size,
		enum dma_data_direction dir, unsigned long attrs)
{
	phys_addr_t phys = page_to_phys(page) + offset;
	dma_addr_t dma_addr = phys_to_dma(dev, phys);

	if (is_swiotlb_force_bounce(dev)) {
		if (is_pci_p2pdma_page(page))
			return DMA_MAPPING_ERROR;
		return swiotlb_map(dev, phys, size, dir, attrs);
	}

	if (unlikely(!dma_capable(dev, dma_addr, size, true))) {
		if (is_pci_p2pdma_page(page))
			return DMA_MAPPING_ERROR;
		if (is_swiotlb_active(dev))
			return swiotlb_map(dev, phys, size, dir, attrs);

		dev_WARN_ONCE(dev, 1,
			     "DMA addr %pad+%zu overflow (mask %llx, bus limit %llx).\n",
			     &dma_addr, size, *dev->dma_mask, dev->bus_dma_limit);
		return DMA_MAPPING_ERROR;
	}

	if (!dev_is_dma_coherent(dev) && !(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		arch_sync_dma_for_device(phys, size, dir);
	return dma_addr;
}

void dma_direct_unmap_page(struct device *dev, dma_addr_t addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	phys_addr_t phys = dma_to_phys(dev, addr);

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		dma_direct_sync_single_for_cpu(dev, addr, size, dir);

	if (unlikely(is_swiotlb_buffer(dev, phys)))
		swiotlb_tbl_unmap_single(dev, phys, size, dir,
					 attrs | DMA_ATTR_SKIP_CPU_SYNC);
}
#endif /* _KERNEL_DMA_DIRECT_H */
