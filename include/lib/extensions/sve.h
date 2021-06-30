/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SVE_H
#define SVE_H

#define fill_sve_helper(num) "ldr z"#num", [%0, #"#num", MUL VL];"
#define read_sve_helper(num) "str z"#num", [%0, #"#num", MUL VL];"

#endif /* SVE_H */