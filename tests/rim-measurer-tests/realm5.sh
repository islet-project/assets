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
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: F33F0F0DA07350AAE833075F23229BF069480A8B191A73E886C2F630D78E137E955DE0E359E05AC08298F165EE8EE68FD5CC6753964A1DDC739BBDBBFF764F5A
