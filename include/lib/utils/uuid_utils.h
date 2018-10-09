/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __UUID_UTILS_H__
#define __UUID_UTILS_H__

#include <stdint.h>
#include <uuid.h>

/* Size (in bytes) of a UUID string as formatted by uuid_to_str() */
#define UUID_STR_SIZE 79

/*
 * Convert a UUID into a string.
 *
 * The caller is responsible for allocating the output string buffer
 * pointed by 'str'. It must be at least UUID_STR_SIZE bytes long.
 *
 * Return the UUID string.
 */
char *uuid_to_str(const uuid_t *uuid, char *str);

/*
 * Return 1 if uuid == uuid_null, 0 otherwise.
 */
unsigned int is_uuid_null(const uuid_t *uuid);

/*
 * Return 1 if uuid1 == uuid2, 0 otherwise.
 */
unsigned int uuid_equal(const uuid_t *uuid1, const uuid_t *uuid2);

/*
 * Take four 32-bit words of data and combine them into a UUID.
 *
 * The caller is responsible for allocating the output UUID variable
 * pointed by 'uuid'.
 *
 * Return the UUID.
 */
uuid_t *make_uuid_from_4words(uuid_t *uuid,
			      uint32_t uuid0,
			      uint32_t uuid1,
			      uint32_t uuid2,
			      uint32_t uuid3);

#endif /* __UUID_UTILS_H__ */
