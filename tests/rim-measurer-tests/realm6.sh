#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on" \
	-d disk.img

# RIM: 64A228664F6DF37137E7C90A2CCC4C926A906C61EB4BC65BAAF16194B08D003F21D9161F3B21BD3131040362EEDF48044B6FAE79D82ABC35B58BFF0ECEB84D64
