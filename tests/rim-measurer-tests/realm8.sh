#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3-its \
	--network virtio \
	--virtio-legacy \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: DCA235359F6864FD3400FADFB2E576045942D125359C6763F51EC51F5A4DE7B0E998E0D082A3636A6968252FFEED00A93E9135E95BBA72D33B976CD0337D109C
