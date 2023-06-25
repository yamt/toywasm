#! /bin/sh

set -e

LLVM_VERSION=${LLVM_VERSION:-13}
ALT_PRIO=100

for c in clang llvm-ar llvm-ranlib ld.lld lld; do
    update-alternatives --remove-all ${c} || :
    update-alternatives --install \
        /usr/bin/${c} \
        ${c} \
        /usr/bin/${c}-${LLVM_VERSION} \
        ${ALT_PRIO}
done
