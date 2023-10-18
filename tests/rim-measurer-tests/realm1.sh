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
       --kaslr-seed 12345                                               \
       --rng                                                            \
       --pmu                                                            \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: A267A8A21ED8874760E7B8A9C7E8286FF9ED6A65EEF232EAF2FFFADBFA940D8A0000000000000000000000000000000000000000000000000000000000000000

