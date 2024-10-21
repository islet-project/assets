#!/bin/sh

#g++ --static -Wl,--no-as-needed -lpthread -ltensorflowlite_flex -ltensorflowlite -o device device.cc socket.cc word_model.cc -L/usr/lib/x86_64-linux-gnu/
make -f build.aarch64.mak CC=aarch64-linux-gnu-g++ EXE=device INC_PATH=/usr/local/include/
#make -f build.aarch64.mak CC=aarch64-linux-gnu-g++ EXE=server INC_PATH=/usr/local/include/
aarch64-linux-gnu-g++ --static -o server.exe server.cc socket.cc
