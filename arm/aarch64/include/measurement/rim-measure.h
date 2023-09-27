#ifndef KVM__RIM_MEASURE_H
#define KVM__RIM_MEASURE_H

#include "kvm/kvm.h"

void measurer_realm_init_ipa_range(u64 start, u64 end);
void measurer_realm_populate(struct kvm *kvm, u64 start, u64 end);
void measurer_realm_configure_hash_algo(uint64_t hash_algo);
void measurer_kvm_arm_realm_create_realm_descriptor(void);
void measurer_reset_vcpu_aarch64(u64 pc, u64 flags, u64 dtb);

void measurer_print_rim(void);

#endif /* KVM__RIM_MEASURE_H */