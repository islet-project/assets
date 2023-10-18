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
       -d disk.img                                                      \
       --balloon                                                        \
       "$@"

# RIM: 332A66036E8130F194CF6363E8A5E591E93DC01DD8CF630D19030FE4B0533F090000000000000000000000000000000000000000000000000000000000000000
