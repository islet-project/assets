#!/bin/sh

./../../lkvm-rim-measurer run                                                              \
       --debug                                                          \
       --realm                                                          \
       --measurement-algo="sha512"                                      \
       --disable-sve                                                    \
       --console virtio                                                 \
       --irqchip=gicv3                                                  \
       -m 128M                                                          \
       -c 1                                                             \
       -k Image.realm                                                   \
       -i initramfs-realm.cpio.gz                                       \
       -p "earlycon=ttyS0 printk.devkmsg=on"                            \
       -n virtio                                                        \
       --rng                                                            \
       --pmu                                                            \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: B355530E7CA966680EC0C1A1386C9D4188E78D108601598A69E0556CB1AD9D00E67629B2D5F429088B2AD4F19AFA26EC3C2851E6D46E277EF8B7249A4E5048A4
