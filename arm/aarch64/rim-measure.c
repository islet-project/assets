/*
* SPDX-License-Identifier: BSD-3-Clause
* SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
*
* The code has been borrowed from TF-RMM (v1.0-Beta0 RMM specification).
*/

#include <string.h>

#include "measurement/measurement.h"
#include "measurement/rim-measure.h"


static enum hash_algo measurer_hash_algo;
static unsigned char rim[MAX_MEASUREMENT_SIZE];

/**
 * These are the initial values passed by KVM for TF-RMM (main) running on FVP.
 * These values depend on the content of RMM's feature0 register (RMI_FEATURES).
 * Note that TF-RMM and Islet return different values for feature0,
 * thus the resulting RIMs even for the same payload are different.
 */
static struct rmi_realm_params realm_params = {
	.s2sz = 0x21, /* Maximum IPA size: 8GB set by host kernel (rme.c: params->s2sz = VTCR_EL2_IPA(kvm->arch.mmu.vtcr))*/
	.num_bps = 2u,
	.num_wps = 2u,
};

static void ripas_granule_measure(unsigned long base,
				  				  unsigned long top)
{
	struct measurement_desc_ripas measure_desc = {0};

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_RIPAS;
	measure_desc.len = sizeof(struct measurement_desc_ripas);
	measure_desc.base = base;
	measure_desc.top = top;
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
		ripas_granule_measure(ipa, ipa + SZ_4K);
	}
}

void measurer_realm_populate(struct kvm *kvm, u64 start, u64 end)
{
	void *data_start = guest_flat_to_host(kvm, start);
	void *data_end = guest_flat_to_host(kvm, end);
	void *data;
	u64 ipa;

	printf("measurer_realm_populate start: 0x%08llx end: 0x%08llx\n", start, end);

	for (data = data_start, ipa = start; data < data_end; data += SZ_4K, ipa += SZ_4K) {
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

void measurer_realm_configure_sve(uint32_t sve_vq)
{
	realm_params.sve_vl = sve_vq;
	realm_params.flags |= RMI_REALM_PARAM_FLAG_SVE;
}

void measurer_realm_configure_pmu(uint32_t num_pmu_cntrs)
{
	realm_params.pmu_num_ctrs = num_pmu_cntrs;
	realm_params.flags |= RMI_REALM_PARAM_FLAG_PMU;
}

void measurer_realm_use_islet(void)
{
	realm_params.num_bps = 0;
	realm_params.num_wps = 0;
}

static void realm_params_measure(void)
{
	/*
	 * Allocate a zero-filled RmiRealmParams data structure
	 * to hold the measured Realm parameters.
	 */
	unsigned char buffer[sizeof(struct rmi_realm_params)] = {0};
	struct rmi_realm_params *rim_params = (struct rmi_realm_params *)buffer;

	/*
	 * Copy the following attributes into the measured Realm
	 * parameters data structure:
	 * - flags
	 * - s2sz
	 * - sve_vl
	 * - num_bps
	 * - num_wps
	 * - pmu_num_ctrs
	 * - hash_algo
	 */
	rim_params->flags = realm_params.flags;
	rim_params->s2sz = realm_params.s2sz;
	rim_params->sve_vl = realm_params.sve_vl;
	rim_params->num_bps = realm_params.num_bps;
	rim_params->num_wps = realm_params.num_wps;
	rim_params->pmu_num_ctrs = realm_params.pmu_num_ctrs;
	rim_params->algorithm = measurer_hash_algo;

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
