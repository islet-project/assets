/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <assert.h>
//#include <smc-rmi.h>
#include <stdbool.h>
#include <stddef.h>
#include <linux/sizes.h>
//#include <utils_def.h>

#define GRANULE_SIZE 4096U

/*
 * Defines member of structure and reserves space
 * for the next member with specified offset.
 */
#define SET_MEMBER(member, start, end)	\
	union {				\
		member;			\
		unsigned char reserved##end[end - start]; \
	}


#define COMPILER_ASSERT(_condition)	\
			extern char compiler_assert[(_condition) ? 1 : -1]

/*#define COMPILER_ASSERT(_condition)	\
			typedef char compiler_assert[(_condition) ? 1 : -1]
*/

/* RmiHashAlgorithm type */
#define RMI_HASH_ALGO_SHA256	0
#define RMI_HASH_ALGO_SHA512	1

/* Supported algorithms */
enum hash_algo {
	HASH_ALGO_SHA256 = RMI_HASH_ALGO_SHA256,
	HASH_ALGO_SHA512 = RMI_HASH_ALGO_SHA512,
};

/*
 * Types of measurement headers as specified in RMM Spec. section C1.1.2
 */
#define MEASUREMENT_REALM_HEADER	(1U)
#define MEASUREMENT_DATA_HEADER		(2U)
#define MEASUREMENT_REC_HEADER		(3U)

/* Measurement slot reserved for RIM */
#define RIM_MEASUREMENT_SLOT		(0U)

/* Maximum number of measurements */
#define MEASUREMENT_SLOT_NR		(5U)

/* Size in bytes of the SHA256 measurement */
#define SHA256_SIZE			(32U)

/* Size in bytes of the SHA512 measurement */
#define SHA512_SIZE			(64U)

#define MEASURE_DESC_TYPE_DATA		0x0
#define MEASURE_DESC_TYPE_REC		0x1
#define MEASURE_DESC_TYPE_RIPAS		0x2

/*
 * Size in bytes of the largest measurement type that can be supported.
 * This macro needs to be updated accordingly if new algorithms are supported.
 */
#define MAX_MEASUREMENT_SIZE		SHA512_SIZE

/* RmmMeasurementDescriptorData type as per RMM spec */
struct measurement_desc_data {
	/* Measurement descriptor type, value 0x0 */
	SET_MEMBER(unsigned char desc_type, 0x0, 0x8);
	/* Length of this data structure in bytes */
	SET_MEMBER(unsigned long len, 0x8, 0x10);
	/* Current RIM value */
	SET_MEMBER(unsigned char rim[MAX_MEASUREMENT_SIZE], 0x10, 0x50);
	/* IPA at which the DATA Granule is mapped in the Realm */
	SET_MEMBER(unsigned long ipa, 0x50, 0x58);
	/* Flags provided by Host */
	SET_MEMBER(unsigned long flags, 0x58, 0x60);
	/*
	 * Hash of contents of DATA Granule, or zero if flags indicate DATA
	 * Granule contents are unmeasured
	 */
	SET_MEMBER(unsigned char content[MAX_MEASUREMENT_SIZE], 0x60, 0x100);
};
COMPILER_ASSERT(sizeof(struct measurement_desc_data) == 0x100);

COMPILER_ASSERT(offsetof(struct measurement_desc_data, desc_type) == 0x0);
COMPILER_ASSERT(offsetof(struct measurement_desc_data, len) == 0x8);
COMPILER_ASSERT(offsetof(struct measurement_desc_data, rim) == 0x10);
COMPILER_ASSERT(offsetof(struct measurement_desc_data, ipa) == 0x50);
COMPILER_ASSERT(offsetof(struct measurement_desc_data, flags) == 0x58);
COMPILER_ASSERT(offsetof(struct measurement_desc_data, content) == 0x60);

/* RmmMeasurementDescriptorRec type as per RMM spec */
struct measurement_desc_rec {
	/* Measurement descriptor type, value 0x1 */
	SET_MEMBER(unsigned char desc_type, 0x0, 0x8);
	/* Length of this data structure in bytes */
	SET_MEMBER(unsigned long len, 0x8, 0x10);
	/* Current RIM value */
	SET_MEMBER(unsigned char rim[MAX_MEASUREMENT_SIZE], 0x10, 0x50);
	/* Hash of 4KB page which contains REC parameters data structure */
	SET_MEMBER(unsigned char content[MAX_MEASUREMENT_SIZE], 0x50, 0x100);
};
COMPILER_ASSERT(sizeof(struct measurement_desc_rec) == 0x100);

COMPILER_ASSERT(offsetof(struct measurement_desc_rec, desc_type) ==  0x0);
COMPILER_ASSERT(offsetof(struct measurement_desc_rec, len) ==  0x8);
COMPILER_ASSERT(offsetof(struct measurement_desc_rec, rim) ==  0x10);
COMPILER_ASSERT(offsetof(struct measurement_desc_rec, content) ==  0x50);

/* RmmMeasurementDescriptorRipas type as per RMM spec */
struct measurement_desc_ripas {
	/* Measurement descriptor type, value 0x2 */
	SET_MEMBER(unsigned char desc_type, 0x0, 0x8);
	/* Length of this data structure in bytes */
	SET_MEMBER(unsigned long len, 0x8, 0x10);
	/* Current RIM value */
	SET_MEMBER(unsigned char rim[MAX_MEASUREMENT_SIZE], 0x10, 0x50);
	/* IPA at which the RIPAS change occurred */
	SET_MEMBER(unsigned long ipa, 0x50, 0x58);
	/* RTT level at which the RIPAS change occurred */
	SET_MEMBER(unsigned char level, 0x58, 0x100);
};
COMPILER_ASSERT(sizeof(struct measurement_desc_ripas) == 0x100);

COMPILER_ASSERT(offsetof(struct measurement_desc_ripas, desc_type) == 0x0);
COMPILER_ASSERT(offsetof(struct measurement_desc_ripas, len) == 0x8);
COMPILER_ASSERT(offsetof(struct measurement_desc_ripas, rim) == 0x10);
COMPILER_ASSERT(offsetof(struct measurement_desc_ripas, ipa) == 0x50);
COMPILER_ASSERT(offsetof(struct measurement_desc_ripas, level) == 0x58);

/* Size of Realm Personalization Value */
#define RPV_SIZE		64
#define SET_MEMBER_RMI	SET_MEMBER

/* RmiDataMeasureContent type */
#define RMI_NO_MEASURE_CONTENT 0
#define RMI_MEASURE_CONTENT  1

/*
 * The Realm attribute parameters are shared by the Host via
 * RMI_REALM_CREATE::params_ptr. The values can be observed or modified
 * either by the Host or by the Realm.
 */
struct rmi_realm_params {
	/* Realm feature register 0 */
	SET_MEMBER_RMI(unsigned long features_0, 0, 0x100);		/* Offset 0 */
	/* Measurement algorithm */
	SET_MEMBER_RMI(unsigned char hash_algo, 0x100, 0x400);	/* 0x100 */
	/* Realm Personalization Value */
	SET_MEMBER_RMI(unsigned char rpv[RPV_SIZE], 0x400, 0x800);	/* 0x400 */
	SET_MEMBER_RMI(struct {
			/* Virtual Machine Identifier */
			unsigned short vmid;			/* 0x800 */
			/* Realm Translation Table base */
			unsigned long rtt_base;			/* 0x808 */
			/* RTT starting level */
			long rtt_level_start;			/* 0x810 */
			/* Number of starting level RTTs */
			unsigned int rtt_num_start;		/* 0x818 */
		   }, 0x800, 0x1000);
};

/*
 * The number of GPRs (starting from X0) that are
 * configured by the host when a REC is created.
 */
#define REC_CREATE_NR_GPRS		8

/* Maximum number of auxiliary granules required for a REC */
#define MAX_REC_AUX_GRANULES		16

struct rmi_rec_params {
	/* Flags */
	SET_MEMBER_RMI(unsigned long flags, 0, 0x100);	/* Offset 0 */
	/* MPIDR of the REC */
	SET_MEMBER_RMI(unsigned long mpidr, 0x100, 0x200);	/* 0x100 */
	/* Program counter */
	SET_MEMBER_RMI(unsigned long pc, 0x200, 0x300);	/* 0x200 */
	/* General-purpose registers */
	SET_MEMBER_RMI(unsigned long gprs[REC_CREATE_NR_GPRS], 0x300, 0x800); /* 0x300 */
	SET_MEMBER_RMI(struct {
			/* Number of auxiliary Granules */
			unsigned long num_aux;			/* 0x800 */
			/* Addresses of auxiliary Granules */
			unsigned long aux[MAX_REC_AUX_GRANULES];/* 0x808 */
		   }, 0x800, 0x1000);
};

/*
 * Calculate the hash of data with algorithm hash_algo to the buffer `out`.
 */
void measurement_hash_compute(enum hash_algo hash_algo,
			      void *data,
			      size_t size, unsigned char *out);

/* Extend a measurement with algorithm hash_algo. */
void measurement_extend(enum hash_algo hash_algo,
			void *current_measurement,
			void *extend_measurement,
			size_t extend_measurement_size,
			unsigned char *out);

/*
 * Return the hash size in bytes for the selected measurement algorithm.
 *
 * Arguments:
 *	- algorithm:	Algorithm to check.
 */
static inline size_t measurement_get_size(
					const enum hash_algo algorithm)
{
	size_t ret = 0;

	switch (algorithm) {
	case HASH_ALGO_SHA256:
		ret = (size_t)SHA256_SIZE;
		break;
	case HASH_ALGO_SHA512:
		ret = (size_t)SHA512_SIZE;
		break;
	default:
		assert(false);
	}

	return ret;
}

#endif /* MEASUREMENT_H */
