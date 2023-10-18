/*
* SPDX-License-Identifier: BSD-3-Clause
* SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
*/

#include <string.h>

#include "measurement/measurement.h"
#include "measurement/rim-measure.h"


static enum hash_algo measurer_hash_algo;
static unsigned char rim[MAX_MEASUREMENT_SIZE];

static void ripas_granule_measure(unsigned long ipa,
				  				  unsigned long level)
{
	struct measurement_desc_ripas measure_desc = {0};

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_RIPAS;
	measure_desc.len = sizeof(struct measurement_desc_ripas);
	measure_desc.ipa = ipa;
	measure_desc.level = level;
	memcpy(measure_desc.rim,
	       rim,
	       measurement_get_size(measurer_hash_algo));

	/*
	 * Hashing the measurement descriptor structure; the result is the
	 * updated RIM.
	 */
	measurement_hash_compute(measurer_hash_algo,
				 &measure_desc,
				 sizeof(measure_desc),
				 rim);
}

static void data_granule_measure(void *data,
				 unsigned long ipa,
				 unsigned long flags)
{
	struct measurement_desc_data measure_desc = {0};

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_DATA;
	measure_desc.len = sizeof(struct measurement_desc_data);
	measure_desc.ipa = ipa;
	measure_desc.flags = flags;
	memcpy(measure_desc.rim,
	       rim,
	       measurement_get_size(measurer_hash_algo));

	if (flags == RMI_MEASURE_CONTENT) {
		/*
		 * Hashing the data granules and store the result in the
		 * measurement descriptor structure.
		 */
		measurement_hash_compute(measurer_hash_algo,
					data,
					GRANULE_SIZE,
					measure_desc.content);
	}

	/*
	 * Hashing the measurement descriptor structure; the result is the
	 * updated RIM.
	 */
	measurement_hash_compute(measurer_hash_algo,
			       &measure_desc,
			       sizeof(measure_desc),
			       rim);
}

static void rec_params_measure(unsigned long pc, unsigned long flags, unsigned long gprs[REC_CREATE_NR_GPRS])
{
	struct measurement_desc_rec measure_desc = {0};
	struct rmi_rec_params rec_params_measured;

	memset(&rec_params_measured, 0, sizeof(rec_params_measured));

	/* Copy the relevant parts of the rmi_rec_params structure to be
	 * measured
	 */
	rec_params_measured.pc = pc;
	rec_params_measured.flags = flags;
	memcpy(&rec_params_measured.gprs,
	       gprs,
	       sizeof(rec_params_measured.gprs));

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_REC;
	measure_desc.len = sizeof(struct measurement_desc_rec);
	memcpy(measure_desc.rim,
	       rim,
	       measurement_get_size(measurer_hash_algo));

	/*
	 * Hashing the REC params structure and store the result in the
	 * measurement descriptor structure.
	 */
	measurement_hash_compute(measurer_hash_algo,
				&rec_params_measured,
				sizeof(rec_params_measured),
				measure_desc.content);

	/*
	 * Hashing the measurement descriptor structure; the result is the
	 * updated RIM.
	 */
	measurement_hash_compute(measurer_hash_algo,
			       &measure_desc,
			       sizeof(measure_desc),
			       rim);
}


void measurer_realm_init_ipa_range(u64 start, u64 end)
{
	u64 ipa;

	for (ipa = start; ipa < end; ipa += SZ_4K) {
		ripas_granule_measure(ipa, 0x3);
	}
}

void measurer_realm_populate(struct kvm *kvm, u64 start, u64 end)
{
	void *data_start = guest_flat_to_host(kvm, start);
	void *data_end = guest_flat_to_host(kvm, end);
	void *data;
	u64 ipa;

	for (data = data_start, ipa = start; data < data_end; data += SZ_4K, ipa += SZ_4K) {
		ripas_granule_measure(ipa, 0x3);
		data_granule_measure(data, ipa, RMI_MEASURE_CONTENT);
	}
}

void measurer_realm_configure_hash_algo(uint64_t hash_algo)
{
	switch (hash_algo) {
		case KVM_CAP_ARM_RME_MEASUREMENT_ALGO_SHA256:
			measurer_hash_algo = HASH_ALGO_SHA256;
			break;
		case KVM_CAP_ARM_RME_MEASUREMENT_ALGO_SHA512:
			measurer_hash_algo = HASH_ALGO_SHA512;
			break;
	}
}

static void realm_params_measure(void)
{
	/* By specification realm_params is 4KB */
	unsigned char buffer[SZ_4K] = {0};
	struct rmi_realm_params *realm_params_measured =
		(struct rmi_realm_params *)&buffer[0];

	realm_params_measured->hash_algo = measurer_hash_algo;

	/* Measure relevant realm params this will be the init value of RIM */
	measurement_hash_compute(measurer_hash_algo,
			       buffer,
			       sizeof(buffer),
			       rim);
}

void measurer_kvm_arm_realm_create_realm_descriptor(void)
{
	realm_params_measure();
}

void measurer_reset_vcpu_aarch64(u64 pc, u64 flags, u64 dtb)
{
	unsigned long gprs[8] = {0,};
	gprs[0] = dtb;
	rec_params_measure(pc, flags, gprs);
}

void measurer_print_rim(void)
{
	size_t i;
	printf("RIM: ");
	for (i = 0; i < MAX_MEASUREMENT_SIZE; i++) {
		printf("%02X", rim[i]);
	}
	printf("\n");
}
