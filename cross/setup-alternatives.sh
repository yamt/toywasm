#! /bin/sh

set -e

LLVM_VERSION=${LLVM_VERSION:-20}
ALT_PRIO=100

COMMANDS=${1:-clang clang++ llvm-ar llvm-ranlib ld.lld lld}

for c in ${COMMANDS}; do
    update-alternatives --remove-all ${c} || :
    update-alternatives --install \
        /usr/bin/${c} \
        ${c} \
        /usr/bin/${c}-${LLVM_VERSION} \
        ${ALT_PRIO}
done
