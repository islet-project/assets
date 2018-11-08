/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPRT_CLIENT_PRIVATE_H
#define SPRT_CLIENT_PRIVATE_H

#include <stdint.h>

struct svc_args {
	u_register_t arg0;
	u_register_t arg1;
	u_register_t arg2;
	u_register_t arg3;
	u_register_t arg4;
	u_register_t arg5;
	u_register_t arg6;
	u_register_t arg7;
};

/*
 * Invoke an SVC call.
 *
 * The arguments to pass through the SVC call must be stored in the svc_args
 * structure. The return values of the SVC call will be stored in the same
 * structure (overriding the input arguments).
 *
 * Returns the first return value. It is equivalent to args.arg0 but is also
 * provided as the return value for convenience.
 */
u_register_t sprt_client_svc(struct svc_args *args);

#endif /* SPRT_CLIENT_PRIVATE_H */
