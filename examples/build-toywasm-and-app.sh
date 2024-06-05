#! /bin/sh

set -e

ROOT=${ROOT:-build}
echo "ROOT: ${ROOT}"

BUILD_TOYWASM=${ROOT}/build-toywasm
BUILD_APP=${ROOT}/build-app

cmake -G Ninja -B ${BUILD_TOYWASM} -D CMAKE_INSTALL_PREFIX=${ROOT} ${TOYWASM_EXTRA_CMAKE_OPTIONS} ../..
cmake --build ${BUILD_TOYWASM} --target install
cmake -G Ninja -B ${BUILD_APP} -D CMAKE_PREFIX_PATH=${ROOT} ${APP_EXTRA_CMAKE_OPTIONS} .
cmake --build ${BUILD_APP}
