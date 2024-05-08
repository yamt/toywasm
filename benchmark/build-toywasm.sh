#! /bin/sh

set -e

# REVISIT: should use full lto?
cmake_config() {
    BUILD_DIR=$1
    shift 1

    # an example to use full lto
    # cmake -B ${BUILD_DIR} -G Ninja . -DUSE_IPO=OFF -DCMAKE_C_FLAGS=-flto=full "$@"

    # an example to use custom llvm
    # cmake -B ${BUILD_DIR}  -G Ninja . -DCUSTOM_LLVM_HOME=/usr/local/opt/llvm@17 "$@"

    cmake -B ${BUILD_DIR}  -G Ninja . "$@"
}

build() {
    BUILD_DIR=$1
    rm -rf ${BUILD_DIR}
    cmake_config "$@"
    cmake --build ${BUILD_DIR}
}

build b
build b.fix -DTOYWASM_USE_SMALL_CELLS=OFF
build b.fix.nosimd -DTOYWASM_USE_SMALL_CELLS=OFF -DTOYWASM_ENABLE_WASM_SIMD=OFF
