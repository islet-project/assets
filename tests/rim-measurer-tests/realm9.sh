#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3-its \
	--network virtio \
	--virtio-transport=mmio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 911E675867EC7A28527E28B5BBA9699DCADE9C5B2A021166C3F441F46F58B366EB2E8DAF1C5337667D4458BC79300944ACE9B2FA89C079A99EBC75A8B579E0B0
