#!/bin/sh

rm -rf build/
mkdir build/
cd build/

aarch64-linux-gnu-gcc -E -P -I../inc/ ../image.ld.S -o image.ld

cmake ..
make

# create a kvmtool-loadable binary
#aarch64-linux-gnu-gcc -E -P -I../inc/ ../image.ld.S -o image.ld
#aarch64-linux-gnu-ld --fatal-warnings -pie --no-dynamic-linker -O1 --gc-sections --build-id=none \
#    -T image.ld -o gateway.elf libgateway.a


#aarch64-linux-gnu-ld -T image.ld -o gateway.elf libgateway.a
#aarch64-linux-gnu-ld -o gateway.elf libgateway.a -entry acs_realm_entry
