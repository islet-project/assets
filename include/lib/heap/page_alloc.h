/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#include <stdint.h>
#include <stdlib.h>

#define HEAP_NULL_PTR		0U
#define HEAP_INVALID_LEN	-1
#define HEAP_OUT_OF_RANGE	-2
#define HEAP_INIT_FAILED	-3
#define HEAP_INIT_SUCCESS	0

/*
 * Initialize the memory heap space to be used
 * @heap_base: heap base address
 * @heap_len: heap size for use
 */
int page_pool_init(uint64_t heap_base, uint64_t heap_len);

/*
 * Return the pointer to the allocated pages
 * @bytes_size: pages to allocate in byte unit
 */
void *page_alloc(u_register_t bytes_size);

/*
 * Reset heap memory usage cursor to heap base address
 */
void page_pool_reset(void);
void page_free(u_register_t ptr);

#endif /* PAGE_ALLOC_H */
