
include common.mk

################################################################################
# Paths to git projects and various binaries
################################################################################
GRUB_CONFIG_PATH	?= $(BUILD_PATH)/fvp/grub
OUT_PATH		?= $(ROOT)/out
GRUB_BIN		?= $(OUT_PATH)/bootaa64.efi
BOOT_IMG		?= $(OUT_PATH)/boot-aosp.img

LINUX_BIN		?= ${OUT_PATH}/Image_aosp
LINUX_DTB_BIN		?= ${OUT_PATH}/fvp-base-aosp.dtb


################################################################################
# Targets
################################################################################
all: boot-img
clean: boot-img-clean

################################################################################
# Boot Image
################################################################################

.PHONY: boot-img
boot-img: boot-img-clean $(GRUB_BIN) ${LINUX_BIN} ${LINUX_DTB_BIN}
	mformat -i $(BOOT_IMG) -n 64 -h 2 -T 131072 -v "BOOT IMG" -C ::
	mcopy -i $(BOOT_IMG) $(LINUX_BIN) ::
	mcopy -i $(BOOT_IMG) $(LINUX_DTB_BIN) ::
	mmd -i $(BOOT_IMG) ::/EFI
	mmd -i $(BOOT_IMG) ::/EFI/BOOT
	mcopy -i $(BOOT_IMG) $(OUT_PATH)/initrd-aosp.img ::
	mcopy -i $(BOOT_IMG) $(GRUB_BIN) ::/EFI/BOOT/bootaa64.efi
	mcopy -i $(BOOT_IMG) $(GRUB_CONFIG_PATH)/grub-aosp.cfg ::/EFI/BOOT/grub.cfg

.PHONY: boot-img-clean
boot-img-clean:
	rm -f $(BOOT_IMG)

