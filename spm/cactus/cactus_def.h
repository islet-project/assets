/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CACTUS_DEF_H
#define CACTUS_DEF_H

#include <utils_def.h>

/*
 * Layout of the Secure Partition image.
 */

/* Up to 2 MiB at an arbitrary address that doesn't overlap the devices. */
#define CACTUS_IMAGE_BASE		ULL(0x1000)
#define CACTUS_IMAGE_SIZE		ULL(0x200000)

/* Memory reserved for stacks */
#define CACTUS_STACKS_SIZE		ULL(0x1000)

/*
 * RX/TX buffer used by VM's in SPM for memory sharing
 * Each VM allocated 2 pages, one for RX and one for TX buffer.
 */
#define CACTUS_RX_BASE			PLAT_CACTUS_RX_BASE
#define CACTUS_TX_BASE			CACTUS_RX_BASE + PAGE_SIZE
#define CACTUS_RX_TX_SIZE		PAGE_SIZE * 2

/*
 * RX/TX buffer helpers.
 */
#define get_sp_rx_start(sp_id) (CACTUS_RX_BASE + (((sp_id & 0x7FFFU) - 1U) * CACTUS_RX_TX_SIZE))
#define get_sp_rx_end(sp_id) (CACTUS_RX_BASE + (((sp_id & 0x7FFFU) - 1U) * CACTUS_RX_TX_SIZE) + PAGE_SIZE)
#define get_sp_tx_start(sp_id) (CACTUS_TX_BASE + (((sp_id & 0x7FFFU) - 1U) * CACTUS_RX_TX_SIZE))
#define get_sp_tx_end(sp_id) (CACTUS_TX_BASE + (((sp_id & 0x7FFFU) - 1U) * CACTUS_RX_TX_SIZE) + PAGE_SIZE)

#endif /* CACTUS_DEF_H */
