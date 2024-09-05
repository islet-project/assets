#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console virtio \
	--irqchip=gicv3 \
	--network virtio \
	--virtio-transport=mmio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: CE409B783BBF0D86CE207E8B9EE8787600AC54BB137F097B5E362B325E4364E1E88EA1DE198D45CC114DC0E1B0691AEA9B5A19A31813AB92A049B1A72AEC90BF

