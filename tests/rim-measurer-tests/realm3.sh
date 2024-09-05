#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--kaslr-seed 12345 \
	--rng \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: CD4329BCE161275AF80D40119847C69C1F02C444E7DD8688050CEDCF8E05CDF04F0D73D9159C75F0BAF50868A8B9B0DA375C06907D1AEAE50B4472AF9A33D682

