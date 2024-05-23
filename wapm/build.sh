#! /bin/sh

cd ..
export BUILD_DIR=wapm/build

# preview1 seems broken on webassembly.sh:
# https://github.com/wasmerio/webassembly.sh/issues/105
export WASI_SDK_MAJOR=8
export WASI_SDK_MINOR=0

rm -rf ${BUILD_DIR}

# Enable all features
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_EXCEPTION_HANDLING=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_EXTENDED_CONST=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_MULTI_MEMORY=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_TAILCALL=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_THREADS=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASI_THREADS=ON"
#EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASI_LITTLEFS=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_USE_USER_SCHED=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_DYLD=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_DYLD_DLFCN=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_EXCEPTION_HANDLING=ON"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DTOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES=ON"
export EXTRA_CMAKE_OPTIONS
./build-wasm32-wasi.sh
