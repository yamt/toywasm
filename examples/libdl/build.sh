#! /bin/sh

set -e
set -x

WASI_SDK=${WASI_SDK:-/opt/wasi-sdk}
WASI_SYSROOT=${WASI_SYSROOT:-${WASI_SDK}/share/wasi-sysroot}
LLVM_HOME=${LLVM_HOME:-${WASI_SDK}}
CC=${LLVM_HOME}/bin/clang
RESOURCE_DIR=${RESOURCE_DIR:-$(${CC} --print-resource-dir)}
CFLAGS="${CFLAGS} --sysroot ${WASI_SYSROOT}"
CFLAGS="${CFLAGS} -resource-dir ${RESOURCE_DIR}"

CFLAGS="${CFLAGS} -O3 -fPIC"

# https://reviews.llvm.org/D155542
#CFLAGS="${CFLAGS} -mextended-const"

# CFLAGS="${CFLAGS} -mtail-call"

CLINKFLAGS="-Xlinker --experimental-pic"
#CLIBLINKFLAGS="-shared -fvisibility=default -mexec-model=reactor"
CLIBLINKFLAGS="-shared -fvisibility=default"

${CC} ${CFLAGS} ${CLINKFLAGS} ${CLIBLINKFLAGS} -o libdl.so libdl.c
