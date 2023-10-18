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
       --9p /shared,FMR                                                 \
       "$@"

# RIM: A850315B35C914C2CC2492212AEBAB37E9D4CA8BDFBA33359FFBD72EAA866936197D39992965930B8F3DBBC76321F416700624C07E6ADBB69CA953BD25D6086F
