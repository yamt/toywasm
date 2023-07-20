#! /bin/sh

set -e

#EXTRA_CMAKE_OPTIONS="-DTOYWASM_USE_JUMP_CACHE=ON -DTOYWASM_USE_JUMP_CACHE2=OFF"

#EXTRA_CMAKE_OPTIONS="-DTOYWASM_ENABLE_WASM_TAILCALL=ON" USE_TAILCALL=ON

export EXTRA_CMAKE_OPTIONS
./build-wasm32-wasi.sh

cmake \
-B build.native \
-DBUILD_TESTING=OFF \
${EXTRA_CMAKE_OPTIONS} \
.
cmake --build build.native

EXTRA_OPTIONS=--print-stats

TOYWASM_NATIVE=${TOYWASM_NATIVE:-./build.native/toywasm}
TOYWASM_WASM=${TOYWASM_WASM:-./build.wasm/toywasm}

echo "=== native ==="
time \
${TOYWASM_NATIVE} --version

echo "=== wasm ==="
time \
${TOYWASM_NATIVE} ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
${TOYWASM_WASM} --version

echo "=== wasm on wasm ==="
time \
${TOYWASM_NATIVE} ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
${TOYWASM_WASM} ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
${TOYWASM_WASM} --version

echo "=== wasm on wasm on wasm ==="
time \
${TOYWASM_NATIVE} ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
${TOYWASM_WASM} ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
${TOYWASM_WASM} ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
${TOYWASM_WASM} --version
