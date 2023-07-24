#! /bin/sh

set -e

DIR=.wcpl

fetch() {
    REPO=https://github.com/false-schemers/wcpl
    REF=458a542ca81fa7a8fd8c8eed38a32dce8ed45135
    mkdir "${DIR}"
    git -C "${DIR}" init
    git -C "${DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${DIR}" checkout FETCH_HEAD
}

if [ ! -d ${DIR} ]; then
    fetch
fi

TOYWASM=${TOYWASM:-toywasm}
TEST_RUNTIME_EXE=${TEST_RUNTIME_EXE:-"${TOYWASM} --wasi --wasi-dir=. --"}

# https://github.com/false-schemers/wcpl#self-hosting
set -x
cd ${DIR}
cc -o wcpl [wcpl].c
./wcpl -o wcpl.wasm [wcpl].c
${TEST_RUNTIME_EXE} wcpl.wasm -o wcpl1.wasm [wcpl].c
diff -s wcpl.wasm wcpl1.wasm
