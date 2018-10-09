/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

	/*
	 * This macro is used to create a function label.
	 */
	.macro func _name
	.type \_name, %function
	.func \_name
	\_name:
	.endm

	/*
	 * This macro is used to mark the end of a function.
	 */
	.macro endfunc _name
	.endfunc
	.size \_name, . - \_name
	.endm
