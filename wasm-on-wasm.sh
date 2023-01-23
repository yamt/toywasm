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

echo "=== native ==="
time \
./build.native/toywasm --version

echo "=== wasm ==="
time \
./build.native/toywasm ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
./build.wasm/toywasm --version

echo "=== wasm on wasm ==="
time \
./build.native/toywasm ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
./build.wasm/toywasm ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
./build.wasm/toywasm --version

echo "=== wasm on wasm on wasm ==="
time \
./build.native/toywasm ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
./build.wasm/toywasm ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
./build.wasm/toywasm ${EXTRA_OPTIONS} --wasi --wasi-dir . -- \
./build.wasm/toywasm --version
