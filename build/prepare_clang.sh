#!/usr/bin/env bash

cd prebuilts

if [ ! -d clang/host/linux-x86/clang-r475365b/bin ]; then
    echo "Downloading Clang from android.googlesource.com ..."
    mkdir -p clang/host/linux-x86/clang-r475365b && cd clang/host/linux-x86/clang-r475365b
    wget -nd https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/master/clang-r475365b.tar.gz
    tar -xzf clang-r475365b.tar.gz
    rm -f clang-r475365b.tar.gz
fi
