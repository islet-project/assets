#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"

#include "asm/realm.h"


static void realm_configure_hash_algo(struct kvm *kvm)
{
	struct kvm_cap_arm_rme_config_item hash_algo_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_HASH_ALGO,
		.hash_algo = kvm->arch.measurement_algo,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&hash_algo_cfg,
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) hash_algo");
}

static void realm_configure_rpv(struct kvm *kvm)
{
	struct kvm_cap_arm_rme_config_item rpv_cfg  = {
		.cfg	= KVM_CAP_ARM_RME_CFG_RPV,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&rpv_cfg,
	};

	if (!kvm->cfg.arch.realm_pv)
		return;

	memset(&rpv_cfg.rpv, 0, sizeof(rpv_cfg.rpv));
	memcpy(&rpv_cfg.rpv, kvm->cfg.arch.realm_pv, strlen(kvm->cfg.arch.realm_pv));

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) RPV");
}

static void realm_configure_sve(struct kvm *kvm)
{
	struct kvm_cap_arm_rme_config_item sve_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_SVE,
		.sve_vq = kvm->arch.sve_vq,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&sve_cfg,
	};

	if (kvm->cfg.arch.disable_sve)
		return;

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) SVE");
}

static void realm_configure_pmu(struct kvm *kvm)
{
	struct kvm_cap_arm_rme_config_item pmu_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_PMU,
		.num_pmu_cntrs = kvm->cfg.arch.pmu_cntrs
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&pmu_cfg,
	};

	if (!kvm->cfg.arch.pmu_cntrs)
		return;

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) PMU");
}

static void realm_configure_debug(struct kvm *kvm)
{
	int n;

	struct kvm_cap_arm_rme_config_item dbg_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_DBG,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&dbg_cfg,
	};

	n = ioctl(kvm->vm_fd, KVM_CHECK_EXTENSION, KVM_CAP_GUEST_DEBUG_HW_BPS);
	if (n < 0)
		die_perror("Failed to get Guest HW BPs");
	dbg_cfg.num_brps = n;

	n = ioctl(kvm->vm_fd, KVM_CHECK_EXTENSION, KVM_CAP_GUEST_DEBUG_HW_WPS);
	if (n < 0)
		die_perror("Failed to get Guest HW BPs");
	dbg_cfg.num_wrps = n;

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) DEBUG");
}

static void realm_configure_parameters(struct kvm *kvm)
{
	realm_configure_hash_algo(kvm);
	realm_configure_rpv(kvm);
	realm_configure_sve(kvm);
	realm_configure_pmu(kvm);
	realm_configure_debug(kvm);
}

void kvm_arm_realm_create_realm_descriptor(struct kvm *kvm)
{
	struct kvm_enable_cap rme_create_rd = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CREATE_RD,
	};

	realm_configure_parameters(kvm);
	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_create_rd) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CREATE_RD)");
}

static void realm_init_ipa_range(struct kvm *kvm, u64 start, u64 size)
{
	struct kvm_cap_arm_rme_init_ipa_args init_ipa_args = {
		.init_ipa_base = start,
		.init_ipa_size = size
	};
	struct kvm_enable_cap rme_init_ipa_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_INIT_IPA_REALM,
		.args[1] = (u64)&init_ipa_args
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_init_ipa_realm) < 0)
		die("unable to intialise IPA range for Realm %llx - %llx (size %llu)",
		    start, start + size, size);
	pr_debug("Initialized IPA range (%llx - %llx) as RAM\n",
		start, start + size);
}

static void realm_populate(struct kvm *kvm, u64 start, u64 size)
{
	struct kvm_cap_arm_rme_populate_realm_args populate_args = {
		.populate_ipa_base = start,
		.populate_ipa_size = size,
		.flags		   = KVM_ARM_RME_POPULATE_FLAGS_MEASURE,
	};
	struct kvm_enable_cap rme_populate_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_POPULATE_REALM,
		.args[1] = (u64)&populate_args
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_populate_realm) < 0)
		die("unable to populate Realm memory %llx - %llx (size %llu)",
		    start, start + size, size);
	pr_debug("Populated Realm memory area : %llx - %llx (size %llu bytes)",
		start, start + size, size);
}

void kvm_arm_realm_populate_metadata(struct kvm *kvm)
{
	if (kvm->arch.metadata == NULL)
		return;

	struct kvm_enable_cap rme_populate_metadata = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_POPULATE_METADATA,
		.args[1] = (u64)kvm->arch.metadata
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_populate_metadata) < 0)
		die("unable to populate the realm metadata %p",
		    kvm->arch.metadata);

	pr_debug("Realm metadata has been populated\n");
}

void kvm_arm_realm_populate_kernel(struct kvm *kvm,
				   unsigned long file_size,
				   unsigned long mem_size)
{
	u64 start, file_end, mem_end;

	start = ALIGN_DOWN(kvm->arch.kern_guest_start, SZ_4K);
	file_end = ALIGN(kvm->arch.kern_guest_start + file_size, SZ_4K);
	mem_end = ALIGN(kvm->arch.kern_guest_start + mem_size, SZ_4K);

	realm_populate(kvm, start, (file_end - start));
	/*
	 * Mark the unpopulated areas of the kernel image as
	 * RAM explicitly.
	  */
	if (file_end < mem_end)
		realm_init_ipa_range(kvm, file_end, (mem_end - file_end));
}

void kvm_arm_realm_populate_initrd(struct kvm *kvm)
{
	u64 start, end;

	start = ALIGN_DOWN(kvm->arch.initrd_guest_start, SZ_4K);
	end = ALIGN(kvm->arch.initrd_guest_start + kvm->arch.initrd_size, SZ_4K);
	realm_populate(kvm, start, end - start);
}

void kvm_arm_realm_populate_dtb(struct kvm *kvm)
{
	u64 start, end;

	start = ALIGN_DOWN(kvm->arch.dtb_guest_start, SZ_4K);
	end = ALIGN(kvm->arch.dtb_guest_start + FDT_MAX_SIZE, SZ_4K);
	realm_populate(kvm, start, end - start);
}

static void kvm_arm_realm_activate_realm(struct kvm *kvm)
{
	struct kvm_enable_cap activate_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_ACTIVATE_REALM,
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &activate_realm) < 0)
		die_perror("KVM_CAP_ARM_RME(KVM_CAP_ARM_RME_ACTIVATE_REALM)");

	kvm->arch.realm_is_active = true;
}

static int kvm_arm_realm_finalize(struct kvm *kvm)
{
	int i;

	if (!kvm__is_realm(kvm))
		return 0;

	/*
	 * VCPU reset must happen before the realm is activated, because their
	 * state is part of the cryptographic measurement for the realm.
	 */
	for (i = 0; i < kvm->nrcpus; i++)
		kvm_cpu__reset_vcpu(kvm->cpus[i]);

	/* Activate and seal the measurement for the realm. */
	kvm_arm_realm_activate_realm(kvm);

	return 0;
}
last_init(kvm_arm_realm_finalize)
