#!/bin/sh

./../../lkvm-rim-measurer run                                                 \
       --debug                                                          \
       --realm                                                          \
       --measurement-algo="sha256"                                      \
       --disable-sve                                                    \
       --console virtio                                                 \
       --irqchip=gicv3                                                  \
       -m 256M                                                          \
       -c 2                                                             \
       -k Image.realm                                                   \
       -i initramfs-realm.cpio.gz                                       \
       -p "earlycon=ttyS0 printk.devkmsg=on"                            \
       -n virtio                                                        \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: 060A994180A48749C280B2833A86A9884A556E0BCC8DE1EB03E10A9B808F3C300000000000000000000000000000000000000000000000000000000000000000

