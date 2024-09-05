#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha256" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--kaslr-seed 12345 \
	--rng \
	--9p /shared,FMR \
	-m 128M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 91D7736609ED227DE884B887684471266AB781C72866DD5D1E50DAED5EFE316D0000000000000000000000000000000000000000000000000000000000000000

