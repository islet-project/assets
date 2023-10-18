#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"

static int debug_fd;

void kvm_cpu__set_debug_fd(int fd)
{
	debug_fd = fd;
}

int kvm_cpu__get_debug_fd(void)
{
	return debug_fd;
}

static struct kvm_arm_target *kvm_arm_generic_target;
static struct kvm_arm_target *kvm_arm_targets[KVM_ARM_NUM_TARGETS];

void kvm_cpu__set_kvm_arm_generic_target(struct kvm_arm_target *target)
{
	kvm_arm_generic_target = target;
}

int kvm_cpu__register_kvm_arm_target(struct kvm_arm_target *target)
{
	unsigned int i = 0;

	for (i = 0; i < ARRAY_SIZE(kvm_arm_targets); ++i) {
		if (!kvm_arm_targets[i]) {
			kvm_arm_targets[i] = target;
			return 0;
		}
	}

	return -ENOSPC;
}

struct kvm_cpu *kvm_cpu__arch_init(struct kvm *kvm, unsigned long cpu_id)
{
	struct kvm_arm_target *target;
	struct kvm_cpu *vcpu;
#ifndef RIM_MEASURE
	int coalesced_offset, mmap_size;
	unsigned int i;
	struct kvm_vcpu_init preferred_init;
#endif
	int err = -1;
	struct kvm_vcpu_init vcpu_init = {
		.features = {},
	};

	vcpu = calloc(1, sizeof(struct kvm_cpu));
	if (!vcpu)
		return NULL;

#ifndef RIM_MEASURE
	vcpu->vcpu_fd = ioctl(kvm->vm_fd, KVM_CREATE_VCPU, cpu_id);
	if (vcpu->vcpu_fd < 0)
		die_perror("KVM_CREATE_VCPU ioctl");

	mmap_size = ioctl(kvm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (mmap_size < 0)
		die_perror("KVM_GET_VCPU_MMAP_SIZE ioctl");

	vcpu->kvm_run = mmap(NULL, mmap_size, PROT_RW, MAP_SHARED,
			     vcpu->vcpu_fd, 0);
	if (vcpu->kvm_run == MAP_FAILED)
		die("unable to mmap vcpu fd");
#endif

	/* VCPU 0 is the boot CPU, the others start in a poweroff state. */
	if (cpu_id > 0)
		vcpu_init.features[0] |= (1UL << KVM_ARM_VCPU_POWER_OFF);

	/* Set KVM_ARM_VCPU_PSCI_0_2 if available */
	if (kvm__supports_extension(kvm, KVM_CAP_ARM_PSCI_0_2)) {
		vcpu_init.features[0] |= (1UL << KVM_ARM_VCPU_PSCI_0_2);
	}

	kvm_cpu__select_features(kvm, &vcpu_init);

#ifndef RIM_MEASURE
	/*
	 * If the preferred target ioctl is successful then
	 * use preferred target else try each and every target type
	 */
	err = ioctl(kvm->vm_fd, KVM_ARM_PREFERRED_TARGET, &preferred_init);
	if (!err) {
		/* Match preferred target CPU type. */
		target = NULL;
		for (i = 0; i < ARRAY_SIZE(kvm_arm_targets); ++i) {
			if (!kvm_arm_targets[i])
				continue;
			if (kvm_arm_targets[i]->id == preferred_init.target) {
				target = kvm_arm_targets[i];
				break;
			}
		}
		if (!target) {
			target = kvm_arm_generic_target;
			vcpu_init.target = preferred_init.target;
		} else {
			vcpu_init.target = target->id;
		}
		err = ioctl(vcpu->vcpu_fd, KVM_ARM_VCPU_INIT, &vcpu_init);
	} else {
		/* Find an appropriate target CPU type. */
		for (i = 0; i < ARRAY_SIZE(kvm_arm_targets); ++i) {
			if (!kvm_arm_targets[i])
				continue;
			target = kvm_arm_targets[i];
			vcpu_init.target = target->id;
			err = ioctl(vcpu->vcpu_fd, KVM_ARM_VCPU_INIT, &vcpu_init);
			if (!err)
				break;
		}
		if (err)
			die("Unable to find matching target");
	}
#else
	target = kvm_arm_generic_target;
	vcpu_init.target = kvm_arm_generic_target->id;
	err = 0;
#endif

	/* Populate the vcpu structure. */
	vcpu->kvm		= kvm;
	vcpu->cpu_id		= cpu_id;
	vcpu->cpu_type		= vcpu_init.target;
	vcpu->cpu_compatible	= target->compatible;
	vcpu->is_running	= true;

	if (err || target->init(vcpu))
		die("Unable to initialise vcpu");

#ifndef RIM_MEASURE
	coalesced_offset = ioctl(kvm->sys_fd, KVM_CHECK_EXTENSION,
				 KVM_CAP_COALESCED_MMIO);
	if (coalesced_offset)
		vcpu->ring = (void *)vcpu->kvm_run +
			     (coalesced_offset * PAGE_SIZE);
#endif

	if (kvm_cpu__configure_features(vcpu))
		die("Unable to configure requested vcpu features");

	return vcpu;
}

void kvm_cpu__arch_nmi(struct kvm_cpu *cpu)
{
}

void kvm_cpu__delete(struct kvm_cpu *vcpu)
{
	kvm_cpu__teardown_pvtime(vcpu->kvm);
	free(vcpu);
}

bool kvm_cpu__handle_exit(struct kvm_cpu *vcpu)
{
	return false;
}

void kvm_cpu__show_page_tables(struct kvm_cpu *vcpu)
{
}

void kvm_cpu__arch_unhandled_mmio(struct kvm_cpu *vcpu)
{
	struct kvm_vcpu_events events = { };

	if (!vcpu->kvm->cfg.arch.is_realm)
		return;

	events.exception.ext_dabt_pending = 1;

	if (ioctl(vcpu->vcpu_fd, KVM_SET_VCPU_EVENTS, &events) < 0)
		die_perror("KVM_SET_VCPU_EVENTS failed");
}
