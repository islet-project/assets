#!/bin/sh

./../../lkvm-rim-measurer run                                                 \
       --debug                                                          \
       --realm                                                          \
       --measurement-algo="sha256"                                      \
       --disable-sve                                                    \
       --console serial                                                 \
       --irqchip=gicv3                                                  \
       -m 256M                                                          \
       -c 2                                                             \
       -k Image.realm                                                   \
       -i initramfs-realm.cpio.gz                                       \
       -p "earlycon=ttyS0 printk.devkmsg=on"                            \
       -n virtio                                                        \
       --9p /shared,FMR                                                 \
       -d disk.img                                                      \
       --balloon                                                        \
       "$@"

# RIM: AECFB8BDA1343D2C75731DDFDB92C9A5759A9F660A47C238D1D2BAD3E25B878D0000000000000000000000000000000000000000000000000000000000000000
