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
       --pmu                                                            \
       --9p /shared,FMR                                                 \
       "$@"

# RIM: 785764A6DCE4D433DA7DA82F8FB7AA8C236CF985266395A7797241443E53DDD2936FF03FE238A3896A322477177F5E5F592E5CA31B01099F57F035DDE418DAF9
