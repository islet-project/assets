#!/bin/sh

./../../lkvm-rim-measurer run                                                 \
       --debug                                                          \
       --realm                                                          \
       --measurement-algo="sha512"                                      \
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

# RIM: F58AF6D6A022F113627B1E0B1E0D9B9A1BFB460207AC29721E84BCEF4B4F5CE08351684444BC11CF329D1D4C807BB621807916C2DF4F56B7326E8D16692546A8

