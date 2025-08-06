#! /bin/sh

set -e

# To build with wasi-threads enabled, backed by pthread
#    WASI_SDK_DIR=... CMAKE_TOOLCHAIN_FILE=${WASI_SDK_DIR}/share/cmake/wasi-sdk-pthread.cmake EXTRA_CMAKE_OPTIONS="-DTOYWASM_ENABLE_WASM_THREADS=ON -DTOYWASM_ENABLE_WASI_THREADS=ON" ./build-wasm32-wasi.sh

# To build with wasi-threads enabled, not backed by pthread
#    EXTRA_CMAKE_OPTIONS="-DTOYWASM_ENABLE_WASM_THREADS=ON -DTOYWASM_ENABLE_WASI_THREADS=ON -DTOYWASM_USE_USER_SCHED=ON" ./build-wasm32-wasi.sh

MAJOR=${WASI_SDK_MAJOR:-26}
MINOR=${WASI_SDK_MINOR:-0}
WASI_SDK_DIR=${WASI_SDK_DIR:-$(pwd)/.wasi-sdk-${MAJOR}.${MINOR}}
CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-${WASI_SDK_DIR}/share/cmake/wasi-sdk.cmake}
DIST_DIR=.dist

BUILD_DIR=${BUILD_DIR:-build.wasm}

mkdir -p ${DIST_DIR}

fetch_wasi_sdk()
{
    UNAME=$(uname -s)
    case ${UNAME} in
    Darwin)
        PLATFORM=macos
        ;;
    Linux)
        PLATFORM=linux
        ;;
    *)
        echo "Unknown uname ${UNAME}"
        exit 1
        ;;
    esac
    MACHINE=
    DIR_SUFFIX=
    if [ ${MAJOR} -ge 23 ]; then
        MACHINE=-$(uname -m)
        DIR_SUFFIX=${MACHINE}-${PLATFORM}
    fi
    TAR=wasi-sdk-${MAJOR}.${MINOR}${MACHINE}-${PLATFORM}.tar.gz
    if [ ! -f ${DIST_DIR}/${TAR} ]; then
        URL=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${MAJOR}/${TAR}
        curl -L -o ${DIST_DIR}/${TAR} ${URL}
    fi
    pax -rz \
    -f ${DIST_DIR}/${TAR} \
    -s"!^wasi-sdk-${MAJOR}\.${MINOR}${DIR_SUFFIX}!${WASI_SDK_DIR}!"
}

test -d "${WASI_SDK_DIR}" || fetch_wasi_sdk

# see also: https://github.com/WebAssembly/tail-call
cmake \
-B ${BUILD_DIR} \
-DWASI_SDK_PREFIX=${WASI_SDK_DIR} \
-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
-DTOYWASM_BUILD_UNITTEST=OFF \
-DTOYWASM_USE_TAILCALL=${USE_TAILCALL:-OFF} \
${EXTRA_CMAKE_OPTIONS} \
.
cmake --build ${BUILD_DIR}
