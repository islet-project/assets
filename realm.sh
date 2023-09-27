#!/bin/sh


./lkvm run                                                              \
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
       --kaslr-seed 12345 \
       --rng \
       --no-pvtime \
	   --pmu \
       --9p /shared,FMR                                                 \
       --dump-dtb rim.dtb \
       "$@"

# Local Variables:
# indent-tabs-mode: nil
# End:
