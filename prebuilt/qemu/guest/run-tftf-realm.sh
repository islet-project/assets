#!/bin/sh

../qemu-system-aarch64 \
    -kernel tftf-realm.elf \
    --enable-kvm \
    -cpu host \
    -smp 1 \
    -m 256M \
    -M virt,gic-version=3 \
    -nographic
