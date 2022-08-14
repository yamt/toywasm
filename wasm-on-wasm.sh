#! /bin/sh

set -e

./build-wasm32-wasi.sh

cmake \
-B build.native \
-DBUILD_TESTING=OFF \
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
