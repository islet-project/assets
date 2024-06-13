#!/bin/sh

OBJCOPY=/home/jinbum/github/islet/assets/toolchain/aarch64-none-elf/bin/aarch64-none-elf-objcopy
OBJDUMP=/home/jinbum/github/islet/assets/toolchain/aarch64-none-elf/bin/aarch64-none-elf-objdump

# build
rm -rf target/
cargo build

# generate .bin
${OBJCOPY} -Obinary ./target/aarch64-unknown-none-softfloat/debug/gateway cvm_gateway.bin
${OBJDUMP} -D ./target/aarch64-unknown-none-softfloat/debug/gateway > cvm_gateway.dump

cp -f cvm_gateway.bin ~/ssd/github/islet/out/shared/cvm_gateway.bin
