#! /bin/sh

TOPDIR=$(cd $(dirname $0) && pwd -P)/..

cd ..
export BUILD_DIR=wapm/build

# preview1 seems broken on webassembly.sh:
# https://github.com/wasmerio/webassembly.sh/issues/105
export WASI_SDK_MAJOR=8
export WASI_SDK_MINOR=0

rm -rf ${BUILD_DIR}

# Enable all features
. ${TOPDIR}/all_features.sh
export EXTRA_CMAKE_OPTIONS
./build-wasm32-wasi.sh
