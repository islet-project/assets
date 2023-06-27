#!/bin/bash

# Build requirements
sudo apt-get install git-core gnupg flex bison build-essential zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 libncurses5 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc unzip fontconfig

# Get repo
./repo init -u https://android.googlesource.com/platform/manifest
./repo init -u https://android.googlesource.com/platform/manifest -b android-13.0.0_r31
# repo sync -j<num_cpu>
./repo sync -j8

# Ensure all git repoitory is downloaded. If no output, go to the next step
./repo diff
