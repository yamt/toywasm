#! /bin/sh

set -e

build() {
    NAME=$1
    shift
    BUILD=b.${NAME}
    cmake -B ${BUILD} $@
    cmake --build ${BUILD}
    cp ${BUILD}/iwasm ~/bin/iwasm.${NAME}
}

COMMON="-DWAMR_BUILD_BULK_MEMORY=1"
# COMMON="${COMMON} -DWAMR_BUILD_SHARED_MEMORY=1"
build classic ${COMMON} -DWAMR_BUILD_FAST_INTERP=0 .
build fast ${COMMON} -DWAMR_BUILD_FAST_INTERP=1 .
build fast-jit ${COMMON} -DWAMR_BUILD_FAST_JIT=1 -DWAMR_BUILD_LAZY_JIT=1 .
build fast-jit-nolazy ${COMMON} -DWAMR_BUILD_FAST_JIT=1 -DWAMR_BUILD_LAZY_JIT=0 .
