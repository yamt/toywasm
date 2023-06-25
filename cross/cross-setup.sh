#! /bin/sh

set -e

dpkg --add-architecture ${ARCH}
apt update
apt install -y crossbuild-essential-${ARCH} libcmocka-dev:${ARCH} wabt clang-13 lld-13

if [ ${ARCH} = i386 ]; then
    apt-get install -y libc6-dev:i386
fi

update-alternatives --install /usr/bin/clang clang /usr/bin/clang-13 50
update-alternatives --install /usr/bin/llvm-ar llvm-ar /usr/bin/llvm-ar-13 50
update-alternatives --install /usr/bin/llvm-ranlib llvm-ranlib /usr/bin/llvm-ranlib-13 50
update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/ld.lld-13 50
update-alternatives --install /usr/bin/lld lld /usr/bin/lld-13 50
