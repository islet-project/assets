#!/bin/sh

aarch64-linux-gnu-gcc --static -o server server.c
aarch64-linux-gnu-gcc --static -o client client.c

sudo cp -f server /home/jinbum/ssd/github/islet/assets/rootfs/rootfs-realm/cloak/
sudo cp -f client /home/jinbum/ssd/github/islet/assets/rootfs/rootfs-realm/cloak/

cp -f server /home/jinbum/ssd/github/islet/out/shared/
cp -f client /home/jinbum/ssd/github/islet/out/shared/
