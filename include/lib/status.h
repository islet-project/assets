/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __STATUS_H__
#define __STATUS_H__

/* Status Code definitions */
#define STATUS_SUCCESS            0x00
#define STATUS_INVALID_PARAMETER  0x01
#define STATUS_UNSUPPORTED        0x02
#define STATUS_OUT_OF_RESOURCES   0x03
#define STATUS_NOT_FOUND          0x04
#define STATUS_ABORTED            0x05
#define STATUS_LOAD_ERROR         0x06
#define STATUS_NEVER_RETURN       0x07
#define STATUS_BUSY               0x08
#define STATUS_NOT_INIT           0x09
#define STATUS_BUFFER_TOO_SMALL   0x0A
#define STATUS_COMPROMISED_DATA   0x0B
#define STATUS_ALREADY_LOADED     0x0C
#define STATUS_FAIL               0x0D

typedef unsigned int STATUS;

#endif /* __STATUS_H__ */
