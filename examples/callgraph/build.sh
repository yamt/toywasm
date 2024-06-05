#! /bin/sh

set -e

# use a debug build to enable assertions for now
TOYWASM_EXTRA_CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=Debug -DTOYWASM_ENABLE_WASM_THREADS=ON" \
../build-toywasm-and-app.sh
