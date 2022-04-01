/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SP_DEF_H
#define SP_DEF_H

#include <utils_def.h>
#include <sp_platform_def.h>

/*
 * Layout of the Secure Partition image.
 */

/* Up to 2 MiB at an arbitrary address that doesn't overlap the devices. */
#define SP_IMAGE_BASE		ULL(0x1000)
#define SP_IMAGE_SIZE		ULL(0x200000)

/* Memory reserved for stacks */
#define SP_STACKS_SIZE		ULL(0x1000)

/*
 * RX/TX buffer used by VM's in SPM for memory sharing
 * Each VM allocated 2 pages, one for RX and one for TX buffer.
 */
#define SP_RX_BASE			PLAT_SP_RX_BASE
#define SP_TX_BASE			SP_RX_BASE + PAGE_SIZE
#define SP_RX_TX_SIZE			PAGE_SIZE * 2

/*
 * RX/TX buffer helpers.
 */
#define get_sp_rx_start(sp_id) (SP_RX_BASE \
				+ (((sp_id & 0x7FFFU) - 1U) * SP_RX_TX_SIZE))
#define get_sp_rx_end(sp_id) (SP_RX_BASE \
			      + (((sp_id & 0x7FFFU) - 1U) * SP_RX_TX_SIZE) \
			      + PAGE_SIZE)
#define get_sp_tx_start(sp_id) (SP_TX_BASE + \
				(((sp_id & 0x7FFFU) - 1U) * SP_RX_TX_SIZE))
#define get_sp_tx_end(sp_id) (SP_TX_BASE \
			      + (((sp_id & 0x7FFFU) - 1U) * SP_RX_TX_SIZE) \
			      + PAGE_SIZE)

#endif /* SP_DEF_H */
