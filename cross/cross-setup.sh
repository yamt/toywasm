#! /bin/sh

set -e

dpkg --add-architecture ${ARCH}
apt update
apt install -y crossbuild-essential-${ARCH} libcmocka-dev:${ARCH} wabt clang-13

if [ ${ARCH} = i386 ]; then
    apt-get install -y libc6-dev:i386
fi
