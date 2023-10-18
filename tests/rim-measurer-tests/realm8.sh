#!/bin/sh

./../../lkvm-rim-measurer run                                                 \
       --debug                                                          \
       --realm                                                          \
       --measurement-algo="sha512"                                      \
       --disable-sve                                                    \
       --console virtio                                                 \
       --irqchip=gicv3                                                  \
       -m 256M                                                          \
       -c 1                                                             \
       -k Image.realm                                                   \
       -i initramfs-realm.cpio.gz                                       \
       -p "earlycon=ttyS0 printk.devkmsg=on"                            \
       -n virtio                                                        \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: 1D5244A1045C98D99657D4230EC6E3AAD9A702F9919D956D13D0C9C6FA9A3AE8399901A5B8F12D7BC9DD175CA9E954E368D9B5682FDAB618BC08225748805D81
