/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CASSERT_H__
#define __CASSERT_H__

/*******************************************************************************
 * Macro to flag a compile time assertion. It uses the preprocessor to generate
 * an invalid C construct if 'cond' evaluates to false.
 * The following  compilation error is triggered if the assertion fails:
 * "error: size of array 'msg' is negative"
 ******************************************************************************/
#define CASSERT(cond, msg)	typedef char msg[(cond) ? 1 : -1]

#endif /* __CASSERT_H__ */
