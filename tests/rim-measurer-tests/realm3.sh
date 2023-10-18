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
       --kaslr-seed 12345                                               \
       --rng                                                            \
       --pmu                                                            \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: 919C20402E7B131F7C02704BAC314C72A44FB37D61AAD41A3CDB3685D500AE7839E2F5DF7F109A592BCA7CD452899EC39699EA3B8CB7EAFC66EFFA5D8F1625C1

