#!/bin/sh

./../../lkvm-rim-measurer run                                                 \
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
       --kaslr-seed 12345                                               \
       --rng                                                            \
       --pmu                                                            \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: 085AB469C3935F6A560368D2A8A17B06D644C415F366B9BBEC59C3BE49EE9AEF6CB9C666A555D2B14E5148C169D915AA5A95662A19D54E94942D52326EC36867
