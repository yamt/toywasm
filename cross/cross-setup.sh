#! /bin/sh

set -e

LLVM_VERSION=13
ALT_PRIO=100

dpkg --add-architecture ${ARCH}
apt update
apt install -y crossbuild-essential-${ARCH} libcmocka-dev:${ARCH} wabt clang-${LLVM_VERSION} lld-${LLVM_VERSION}

if [ ${ARCH} = i386 ]; then
    apt-get install -y libc6-dev:i386
fi

for c in clang llvm-ar llvm-ranlib ld.lld lld; do
    update-alternatives --install \
        /usr/bin/${c} \
        ${c} \
        /usr/bin/${c}-${LLVM_VERSION} \
        ${ALT_PRIO}
done
