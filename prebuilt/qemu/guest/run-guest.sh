#!/bin/sh

function usage()
{
	echo "Usage: $0 <Image>"
	exit 1
}

if [ $# -lt 1 ]; then
	usage
fi

qemu-system-aarch64 \
	-kernel $1 \
	-initrd initramfs-busybox-aarch64.cpio.gz \
	--enable-kvm \
	-cpu host \
	-smp 4 \
	-M virt,gic-version=3 \
	-m 256 \
	-serial mon:stdio \
	-nographic \
	-device virtio-serial-pci \
	-device virtio-net-pci,disable-modern=on,netdev=vnet,mac=22:33:00:22:34:56,romfile=./efi-virtio.rom \
	-netdev tap,id=vnet,ifname=tap0,script=no \
#	-device virtio-blk-pci,disable-modern=on,drive=vdisk,romfile=./efi-virtio.rom \
#	-drive if=none,id=vdisk,file=vm0-disk.img,format=raw
