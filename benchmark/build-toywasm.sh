#! /bin/sh

set -e

rm -rf b
rm -rf b.fix
rm -rf b.fix.nosimd

# REVISIT: should use full lto?
#cmake -B b -G Ninja . -DUSE_IPO=OFF -DCMAKE_C_FLAGS=-flto=full
cmake -B b -G Ninja .
cmake --build b
#exit 0

cmake -B b.fix -G Ninja -DTOYWASM_USE_SMALL_CELLS=OFF .
cmake --build b.fix

cmake -B b.fix.nosimd -G Ninja -DTOYWASM_USE_SMALL_CELLS=OFF -DTOYWASM_ENABLE_WASM_SIMD=OFF .
cmake --build b.fix.nosimd
