#!/bin/sh

make CROSS_COMPILE=../../assets/toolchain/aarch64-none-linux-gnu-10-2/bin/aarch64-none-linux-gnu- ARCH=arm64 CLOAK_VM=yes LIBFDT_DIR=../../assets/dtc/libfdt lkvm_vm-static

