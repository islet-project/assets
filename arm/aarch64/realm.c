#include "kvm/kvm.h"

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

static void realm_configure_parameters(struct kvm *kvm)
{
	realm_configure_hash_algo(kvm);
	realm_configure_rpv(kvm);
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
