#! /bin/sh

set -e

LLVM_VERSION=18

dpkg --add-architecture ${ARCH}
apt update
apt install -y crossbuild-essential-${ARCH} libcmocka-dev:${ARCH} wabt clang-${LLVM_VERSION} lld-${LLVM_VERSION}

if [ ${ARCH} = i386 ]; then
    apt-get install -y libc6-dev:i386
fi
