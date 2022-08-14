#! /bin/sh

cd ..
export BUILD_DIR=wapm/build

# preview1 seems broken on webassembly.sh:
# https://github.com/wasmerio/webassembly.sh/issues/105
export WASI_SDK_MAJOR=8
export WASI_SDK_MINOR=0

rm -rf ${BUILD_DIR}

./build-wasm32-wasi.sh
