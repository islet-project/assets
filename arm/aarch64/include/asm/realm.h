/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ASM_REALM_H
#define __ASM_REALM_H

#include "kvm/kvm.h"

void kvm_arm_realm_create_realm_descriptor(struct kvm *kvm);

#endif /* ! __ASM_REALM_H */
