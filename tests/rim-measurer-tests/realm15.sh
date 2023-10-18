#!/bin/sh

./../../lkvm-rim-measurer run                                                 \
       --debug                                                          \
       --realm                                                          \
       --measurement-algo="sha256"                                      \
       --disable-sve                                                    \
       --console serial                                                 \
       --irqchip=gicv3-its                                              \
       -m 256M                                                          \
       -c 2                                                             \
       -k Image.realm                                                   \
       -i initramfs-realm.cpio.gz                                       \
       -p "earlycon=ttyS0 printk.devkmsg=on"                            \
       -n virtio                                                        \
       --9p /shared,FMR                                                 \
       --virtio-legacy                                                  \
       "$@"

# RIM: C31CBBA7A052CC462764BE24ADFBF90F47ECC56D6A67D2183937FB07A1F9C2640000000000000000000000000000000000000000000000000000000000000000
