#ifndef KVM__KVM_ARCH_H
#define KVM__KVM_ARCH_H

#include <linux/sizes.h>
#include <linux/byteorder.h>
#include <string.h>
#include <unistd.h>

#include <asm/image.h>

struct kvm;
unsigned long long kvm__arch_get_kern_offset(struct kvm *kvm, int fd);
int kvm__arch_get_ipa_limit(struct kvm *kvm);
void kvm__arch_enable_mte(struct kvm *kvm);

static inline bool is_arm64_image(const void *header)
{
	const struct arm64_image_header *hdr = header;

	return memcmp(&hdr->magic, ARM64_IMAGE_MAGIC, sizeof(hdr->magic)) == 0;
}

static inline ssize_t arm64_image_size(const void *header)
{
	const struct arm64_image_header *hdr = header;

	return le64_to_cpu(hdr->image_size);
}

static inline ssize_t arm64_image_text_offset(const void *header)
{
	const struct arm64_image_header *hdr = header;

	return le64_to_cpu(hdr->text_offset);
}

#define MAX_PAGE_SIZE	SZ_64K

#define ARCH_HAS_CFG_RAM_ADDRESS	1

#include "arm-common/kvm-arch.h"

#endif /* KVM__KVM_ARCH_H */
