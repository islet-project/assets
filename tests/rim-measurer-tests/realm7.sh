#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha256" \
	--disable-sve \
	--console virtio \
	--irqchip=gicv3 \
	--network virtio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 5881D6C9DB2CF8A9AF3A7E3EA4CA9600722469787BE0405E76421B2C7AE596E2
